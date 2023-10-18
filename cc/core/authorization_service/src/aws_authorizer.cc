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

#include "aws_authorizer.h"

#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/base64.h>

#include "core/http2_client/src/aws/aws_v4_signer.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/http_types.h"
#include "core/utils/src/base64.h"

#include "error_codes.h"

using boost::system::error_code;
using std::begin;
using std::bind;
using std::end;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::move;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using json = nlohmann::json;
using google::scp::core::common::AutoExpiryConcurrentMap;
using google::scp::core::utils::Base64Decode;
using nghttp2::asio_http2::host_service_from_uri;
using std::function;
using std::shared_ptr;

static constexpr const char kAWSAuthorizer[] = "AWSAuthorizer";
static constexpr const char kAccessKey[] = "access_key";
static constexpr const char kSignature[] = "signature";
static constexpr const char kAmzDate[] = "amz_date";
static constexpr const char kSecurityToken[] = "security_token";
static constexpr const char kAuthorizedDomain[] = "authorized_domain";
static constexpr const char kHostHeader[] = "Host";
static constexpr const char* kSignedHeaders[] = {"Host", "X-Amz-Date"};
static constexpr int kAuthorizationTokenCacheLifetimeSeconds = 150;

namespace google::scp::core {

AwsAuthorizer::AwsAuthorizer(
    const string& server_endpoint, const string& aws_region,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<HttpClientInterface>& http_client)
    : authorization_tokens_(
          make_unique<AutoExpiryConcurrentMap<
              string, shared_ptr<AwsAuthorizationTokenCacheEntry>>>(
              kAuthorizationTokenCacheLifetimeSeconds,
              false /* extend_entry_lifetime_on_access */,
              false /* block_entry_while_eviction */,
              bind(&AwsAuthorizer::OnBeforeGarbageCollection, this, _1, _2, _3),
              async_executor)),
      server_endpoint_(make_shared<string>(server_endpoint)),
      aws_region_(aws_region),
      http_client_(http_client) {}

ExecutionResult AwsAuthorizer::Init() noexcept {
  return authorization_tokens_->Init();
};

ExecutionResult AwsAuthorizer::Run() noexcept {
  return authorization_tokens_->Run();
}

ExecutionResult AwsAuthorizer::Stop() noexcept {
  return authorization_tokens_->Stop();
}

void AwsAuthorizer::OnBeforeGarbageCollection(
    string& token, shared_ptr<AwsAuthorizationTokenCacheEntry>& transaction,
    function<void(bool)> should_delete_entry) noexcept {
  // TODO: Enable pre-expiration refresh.
  should_delete_entry(true);
}

ExecutionResult AwsAuthorizer::Authorize(
    AsyncContext<AuthorizationRequest, AuthorizationResponse>&
        authorization_context) noexcept {
  if (!authorization_context.request) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }
  const auto& request = *authorization_context.request;
  if (!request.authorization_token ||
      request.authorization_token->length() == 0) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  if (!request.claimed_identity || request.claimed_identity->length() == 0) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  string token;
  auto execution_result = Base64Decode(*request.authorization_token, token);
  if (!execution_result) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  json json_token;
  try {
    json_token = json::parse(token);
  } catch (...) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }
  // Check if all the required fields are present
  if (!json_token.contains(kAccessKey) || !json_token.contains(kSignature) ||
      !json_token.contains(kAmzDate)) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
  }

  string access_key = json_token[kAccessKey].get<string>();
  string signature = json_token[kSignature].get<string>();
  string amz_date = json_token[kAmzDate].get<string>();
  string security_token;
  if (json_token.contains(kSecurityToken)) {
    security_token = json_token[kSecurityToken].get<string>();
  }
  auto http_request = make_shared<HttpRequest>();
  http_request->method = HttpMethod::POST;
  http_request->path = server_endpoint_;
  http_request->headers = make_shared<HttpHeaders>();

  error_code http2_error_code;
  string scheme;
  string host;
  string service;
  if (host_service_from_uri(http2_error_code, scheme, host, service,
                            *server_endpoint_)) {
    return FailureExecutionResult(
        errors::SC_AUTHORIZATION_SERVICE_INVALID_CONFIG);
  }

  auto authorization_cache_entry =
      make_shared<AwsAuthorizationTokenCacheEntry>();
  auto cache_entry_key = *request.claimed_identity + ":" + token;
  auto pair = make_pair(cache_entry_key, authorization_cache_entry);
  execution_result =
      authorization_tokens_->Insert(pair, authorization_cache_entry);
  if (!execution_result.Successful()) {
    if (execution_result.status_code !=
            errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED &&
        execution_result.status_code !=
            errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS) {
      return execution_result;
    }

    if (execution_result.status_code ==
        errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED) {
      return RetryExecutionResult(execution_result.status_code);
    }

    if (authorization_cache_entry->is_loaded) {
      authorization_context.response = make_shared<AuthorizationResponse>();
      authorization_context.response->authorized_domain =
          authorization_cache_entry->authorized_domain;
      authorization_context.result = SuccessExecutionResult();
      authorization_context.Finish();
      return SuccessExecutionResult();
    }

    return RetryExecutionResult(
        errors::SC_AUTHORIZATION_SERVICE_AUTH_TOKEN_IS_REFRESHING);
  }

  execution_result = authorization_tokens_->DisableEviction(pair.first);
  if (!execution_result.Successful()) {
    return RetryExecutionResult(
        errors::SC_AUTHORIZATION_SERVICE_AUTH_TOKEN_IS_REFRESHING);
  }

  http_request->headers->insert({string(kHostHeader), host});
  http_request->headers->insert(
      {string(kClaimedIdentityHeader),
       *authorization_context.request->claimed_identity});

  AwsV4Signer signer(access_key, "", security_token, "execute-api",
                     aws_region_);
  vector<string> headers_to_sign{begin(kSignedHeaders), end(kSignedHeaders)};
  execution_result = signer.SignRequestWithSignature(
      *http_request, headers_to_sign, amz_date, signature);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  AsyncContext<HttpRequest, HttpResponse> http_context(
      move(http_request),
      bind(&AwsAuthorizer::HandleHttpResponse, this, authorization_context,
           cache_entry_key, _1),
      authorization_context);
  auto result = http_client_->PerformRequest(http_context);
  if (!result.Successful()) {
    authorization_tokens_->Erase(cache_entry_key);
    return result;
  }

  return SuccessExecutionResult();
}

void AwsAuthorizer::HandleHttpResponse(
    AsyncContext<AuthorizationRequest, AuthorizationResponse>&
        authorization_context,
    std::string& cache_entry_key,
    AsyncContext<HttpRequest, HttpResponse>& http_context) {
  // If http request wasn't successful, return unauthorized error.
  // TODO: differentiate server errors and HTTP 4xx errors.
  if (!http_context.result) {
    authorization_tokens_->Erase(cache_entry_key);
    authorization_context.result = http_context.result;
    authorization_context.Finish();
    return;
  }

  const auto& http_body_bytes = http_context.response->body.bytes;
  string body_str(http_body_bytes->data(), http_body_bytes->size());
  json body_json;
  bool parse_fail = true;
  try {
    body_json = json::parse(body_str);
    parse_fail = false;
  } catch (...) {}
  if (parse_fail || !body_json.contains(kAuthorizedDomain)) {
    authorization_context.result =
        FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_INTERNAL_ERROR);
    authorization_context.Finish();
    return;
  }

  authorization_context.response = make_shared<AuthorizationResponse>();
  authorization_context.response->authorized_domain =
      make_shared<AuthorizedDomain>(body_json[kAuthorizedDomain].get<string>());

  shared_ptr<AwsAuthorizationTokenCacheEntry> auth_token_cache_entry;
  auto execution_result =
      authorization_tokens_->Find(cache_entry_key, auth_token_cache_entry);
  if (!execution_result.Successful()) {
    SCP_DEBUG_CONTEXT(kAWSAuthorizer, authorization_context,
                      "Cannot find the cached token.");
  } else {
    auth_token_cache_entry->authorized_domain =
        authorization_context.response->authorized_domain;
    auth_token_cache_entry->is_loaded = true;

    execution_result = authorization_tokens_->EnableEviction(cache_entry_key);
    if (!execution_result.Successful()) {
      authorization_tokens_->Erase(cache_entry_key);
    }
  }

  authorization_context.result = SuccessExecutionResult();
  authorization_context.Finish();
}
}  // namespace google::scp::core
