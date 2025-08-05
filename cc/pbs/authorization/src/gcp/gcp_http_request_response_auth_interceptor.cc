/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cc/pbs/authorization/src/gcp/gcp_http_request_response_auth_interceptor.h"

#include <stddef.h>

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cc/core/authorization_service/src/error_codes.h"
#include "cc/core/interface/type_def.h"
#include "cc/core/utils/src/base64.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs {
namespace {

using ::privacy_sandbox::pbs_common::AuthorizationMetadata;
using ::privacy_sandbox::pbs_common::AuthorizedDomain;
using ::privacy_sandbox::pbs_common::AuthorizedMetadata;
using ::privacy_sandbox::pbs_common::Base64Decode;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::HttpRequest;
using ::privacy_sandbox::pbs_common::HttpResponse;
using ::privacy_sandbox::pbs_common::kClaimedIdentityHeader;
using ::privacy_sandbox::pbs_common::PadBase64Encoding;
using ::privacy_sandbox::pbs_common::RetryExecutionResult;
using ::privacy_sandbox::pbs_common::SC_AUTHORIZATION_SERVICE_BAD_TOKEN;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using json = nlohmann::json;

constexpr char kAuthorizationHeader[] = "Authorization";

constexpr char kAuthHeaderFormat[] = "Bearer %s";

constexpr char kAuthorizedDomain[] = "authorized_domain";

constexpr size_t kIdTokenParts = 3;

const std::vector<std::string>& GetRequiredJWTComponents() {
  static const auto* const components =
      new std::vector<std::string>{"iss", "aud", "sub", "iat", "exp"};
  return *components;
}

}  // namespace

ExecutionResult GcpHttpRequestResponseAuthInterceptor::PrepareRequest(
    const AuthorizationMetadata& authorization_metadata,
    HttpRequest& http_request) {
  if (authorization_metadata.authorization_token.length() == 0 ||
      authorization_metadata.claimed_identity.length() == 0) {
    return FailureExecutionResult(SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  // The token is split like so: <HEADER>.<PAYLOAD>.<SIGNATURE>
  std::vector<std::string> parts =
      absl::StrSplit(authorization_metadata.authorization_token, '.');
  if (parts.size() != kIdTokenParts) {
    return FailureExecutionResult(SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  // The JSON Web Token (JWT) lives in the middle (1) part of the whole
  // string.
  // Padding (if needed) is done so that decode works
  const auto& json_web_token = parts[1];
  auto padded_token_or = PadBase64Encoding(json_web_token);
  if (!padded_token_or.Successful()) {
    return padded_token_or.result();
  }

  std::string token;
  auto execution_result = Base64Decode(*padded_token_or, token);
  if (!execution_result) {
    return FailureExecutionResult(SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  json json_token;
  try {
    json_token = json::parse(token);
  } catch (...) {
    return FailureExecutionResult(SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }
  // Check if all the required fields are present
  if (!std::all_of(GetRequiredJWTComponents().begin(),
                   GetRequiredJWTComponents().end(),
                   [&json_token](const auto& component) {
                     return json_token.contains(component);
                   })) {
    return FailureExecutionResult(SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  http_request.headers->insert({std::string(kClaimedIdentityHeader),
                                authorization_metadata.claimed_identity});

  http_request.headers->insert(
      {std::string(kAuthorizationHeader),
       absl::StrFormat(kAuthHeaderFormat,
                       authorization_metadata.authorization_token)});

  return SuccessExecutionResult();
}

ExecutionResultOr<AuthorizedMetadata>
GcpHttpRequestResponseAuthInterceptor::ObtainAuthorizedMetadataFromResponse(
    const AuthorizationMetadata&, const HttpResponse& http_response) {
  std::string body_str = http_response.body.ToString();
  json body_json;
  bool parse_fail = true;
  try {
    body_json = json::parse(body_str);
    parse_fail = false;
  } catch (...) {}
  if (parse_fail || !body_json.contains(kAuthorizedDomain)) {
    return FailureExecutionResult(SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  return AuthorizedMetadata{
      .authorized_domain = std::make_shared<AuthorizedDomain>(
          body_json[kAuthorizedDomain].get<std::string>())};
}

}  // namespace privacy_sandbox::pbs
