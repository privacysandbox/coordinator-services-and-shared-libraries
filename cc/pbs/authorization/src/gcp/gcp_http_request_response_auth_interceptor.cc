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

#include "gcp_http_request_response_auth_interceptor.h"

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
#include "core/authorization_service/src/error_codes.h"
#include "core/interface/type_def.h"
#include "core/utils/src/base64.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::AuthorizationMetadata;
using google::scp::core::AuthorizedMetadata;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::PadBase64Encoding;
using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

using json = nlohmann::json;

namespace google::scp::pbs {
namespace {

constexpr char kAuthorizationHeader[] = "Authorization";

constexpr char kAuthHeaderFormat[] = "Bearer %s";

constexpr char kAuthorizedDomain[] = "authorized_domain";

constexpr size_t kIdTokenParts = 3;

const vector<string>& GetRequiredJWTComponents() {
  static const auto* const components =
      new vector<string>{"iss", "aud", "sub", "iat", "exp"};
  return *components;
}

}  // namespace

ExecutionResult GcpHttpRequestResponseAuthInterceptor::PrepareRequest(
    const AuthorizationMetadata& authorization_metadata,
    HttpRequest& http_request) {
  if (authorization_metadata.authorization_token.length() == 0 ||
      authorization_metadata.claimed_identity.length() == 0) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  // The token is split like so: <HEADER>.<PAYLOAD>.<SIGNATURE>
  vector<string> parts =
      absl::StrSplit(authorization_metadata.authorization_token, '.');
  if (parts.size() != kIdTokenParts) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  // The JSON Web Token (JWT) lives in the middle (1) part of the whole
  // string.
  // Padding (if needed) is done so that decode works
  const auto& json_web_token = parts[1];
  auto padded_token_or = PadBase64Encoding(json_web_token);
  if (!padded_token_or.Successful()) {
    return padded_token_or.result();
  }

  string token;
  auto execution_result = Base64Decode(*padded_token_or, token);
  if (!execution_result) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  json json_token;
  try {
    json_token = json::parse(token);
  } catch (...) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }
  // Check if all the required fields are present
  if (!std::all_of(GetRequiredJWTComponents().begin(),
                   GetRequiredJWTComponents().end(),
                   [&json_token](const auto& component) {
                     return json_token.contains(component);
                   })) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  http_request.headers->insert({string(core::kClaimedIdentityHeader),
                                authorization_metadata.claimed_identity});

  http_request.headers->insert(
      {string(kAuthorizationHeader),
       absl::StrFormat(kAuthHeaderFormat,
                       authorization_metadata.authorization_token)});

  if (enable_site_based_authorization_) {
    http_request.headers->insert(
        {std::string(core::kEnablePerSiteEnrollmentHeader), "true"});
  }

  return SuccessExecutionResult();
}

core::ExecutionResultOr<AuthorizedMetadata>
GcpHttpRequestResponseAuthInterceptor::ObtainAuthorizedMetadataFromResponse(
    const AuthorizationMetadata&, const HttpResponse& http_response) {
  string body_str = http_response.body.ToString();
  json body_json;
  bool parse_fail = true;
  try {
    body_json = json::parse(body_str);
    parse_fail = false;
  } catch (...) {}
  if (parse_fail || !body_json.contains(kAuthorizedDomain)) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  return AuthorizedMetadata{
      .authorized_domain = make_shared<core::AuthorizedDomain>(
          body_json[kAuthorizedDomain].get<string>())};
}

}  // namespace google::scp::pbs
