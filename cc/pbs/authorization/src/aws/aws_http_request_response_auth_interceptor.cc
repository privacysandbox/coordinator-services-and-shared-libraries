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

#include "aws_http_request_response_auth_interceptor.h"

#include <array>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/authorization_service/src/error_codes.h"
#include "core/http2_client/src/aws/aws_v4_signer.h"
#include "core/interface/type_def.h"
#include "core/utils/src/base64.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::AuthorizationMetadata;
using google::scp::core::AuthorizedMetadata;
using google::scp::core::AwsV4Signer;
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

using json = nlohmann::json;

namespace google::scp::pbs {
namespace {

constexpr char kAccessKey[] = "access_key";
constexpr char kSignature[] = "signature";
constexpr char kAmzDate[] = "amz_date";
constexpr char kAuthorizedDomain[] = "authorized_domain";
constexpr char kSecurityToken[] = "security_token";
constexpr std::array kSignedHeaders = {"Host", "X-Amz-Date"};

}  // namespace

ExecutionResult AwsHttpRequestResponseAuthInterceptor::PrepareRequest(
    const AuthorizationMetadata& authorization_metadata,
    HttpRequest& http_request) {
  if (authorization_metadata.authorization_token.length() == 0) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  if (authorization_metadata.claimed_identity.length() == 0) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  // Padding (if needed) is done so that decode works
  auto padded_token_or =
      PadBase64Encoding(authorization_metadata.authorization_token);
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
  if (!json_token.contains(kAccessKey) || !json_token.contains(kSignature) ||
      !json_token.contains(kAmzDate)) {
    return FailureExecutionResult(
        core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }
  const string& access_key = json_token[kAccessKey].get<string>();
  const string& signature = json_token[kSignature].get<string>();
  const string& amz_date = json_token[kAmzDate].get<string>();
  const string& security_token = [&json_token]() {
    if (json_token.contains(kSecurityToken)) {
      return json_token[kSecurityToken].get<string>();
    }
    return string();
  }();

  http_request.headers->insert({string(core::kClaimedIdentityHeader),
                                authorization_metadata.claimed_identity});
  if (enable_site_based_authorization_) {
    http_request.headers->insert(
        {std::string(core::kEnablePerSiteEnrollmentHeader), "true"});
  }

  AwsV4Signer signer(access_key, "", security_token, "execute-api",
                     aws_region_);
  std::vector<string> headers_to_sign{std::begin(kSignedHeaders),
                                      std::end(kSignedHeaders)};
  return signer.SignRequestWithSignature(http_request, headers_to_sign,
                                         amz_date, signature);
}

core::ExecutionResultOr<AuthorizedMetadata>
AwsHttpRequestResponseAuthInterceptor::ObtainAuthorizedMetadataFromResponse(
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
