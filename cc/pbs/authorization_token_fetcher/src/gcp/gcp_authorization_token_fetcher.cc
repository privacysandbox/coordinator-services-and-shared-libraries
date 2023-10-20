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
#include "gcp_authorization_token_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "core/authorization_service/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/utils/src/base64.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FetchTokenRequest;
using google::scp::core::FetchTokenResponse;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::PadBase64Encoding;
using nlohmann::json;
using std::bind;
using std::cbegin;
using std::cend;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::move;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::chrono::seconds;
using std::placeholders::_1;

namespace google::scp::pbs {
namespace {

constexpr char kGcpAuthTokenFetcher[] = "GcpAuthorizationTokenFetcher";

// This is not HTTPS but this is still safe according to the docs:
// https://cloud.google.com/compute/docs/metadata/overview#metadata_security_considerations
constexpr char kTokenServerPath[] =
    "http://metadata/computeMetadata/v1/instance/service-accounts/default/"
    "identity";
constexpr char kAudienceParameter[] = "audience=";
constexpr char kFormatFullParameter[] = "format=full";
constexpr char kMetadataFlavorHeader[] = "Metadata-Flavor";
constexpr char kMetadataFlavorHeaderValue[] = "Google";

constexpr size_t kExpectedTokenPartsSize = 3;
constexpr char kJsonTokenIssuerKey[] = "iss";
constexpr char kJsonTokenAudienceKey[] = "aud";
constexpr char kJsonTokenSubjectKey[] = "sub";
constexpr char kJsonTokenIssuedAtKey[] = "iat";
constexpr char kJsonTokenExpiryKey[] = "exp";

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredJWTComponents() {
  static char const* components[5];
  using iterator_type = decltype(cbegin(components));
  static pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kJsonTokenIssuerKey;
    components[1] = kJsonTokenAudienceKey;
    components[2] = kJsonTokenSubjectKey;
    components[3] = kJsonTokenIssuedAtKey;
    components[4] = kJsonTokenExpiryKey;
    return make_pair(cbegin(components), cend(components));
  }();
  return iterator_pair;
}

}  // namespace

void GcpAuthorizationTokenFetcher::ProcessHttpResponse(
    AsyncContext<FetchTokenRequest, FetchTokenResponse> fetch_token_context,
    AsyncContext<HttpRequest, HttpResponse> http_context) {
  fetch_token_context.result = http_context.result;
  if (!http_context.result.Successful()) {
    FinishContext(http_context.result, fetch_token_context, async_executor_);
    return;
  }
  vector<string> token_parts =
      absl::StrSplit(http_context.response->body.ToString(), '.');
  if (token_parts.size() != kExpectedTokenPartsSize) {
    auto result =
        RetryExecutionResult(core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
    SCP_ERROR_CONTEXT(kGcpAuthTokenFetcher, fetch_token_context, result,
                      "Received token does not have %d parts.",
                      kExpectedTokenPartsSize);
    FinishContext(result, fetch_token_context, async_executor_);
    return;
  }

  // The JSON Web Token (JWT) lives in the middle (1) part of the whole
  // string.
  auto padded_jwt_or = PadBase64Encoding(token_parts[1]);
  if (!padded_jwt_or.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenFetcher, fetch_token_context,
                      padded_jwt_or.result(),
                      "Received JWT cannot be padded correctly.");
    FinishContext(padded_jwt_or.result(), fetch_token_context, async_executor_);
    return;
  }
  string decoded_json_str;
  if (auto decode_result = Base64Decode(*padded_jwt_or, decoded_json_str);
      !decode_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenFetcher, fetch_token_context, decode_result,
                      "Received token JWT could not be decoded.");
    FinishContext(decode_result, fetch_token_context, async_executor_);
    return;
  }

  json json_web_token;
  try {
    json_web_token = json::parse(decoded_json_str);
  } catch (...) {
    auto result =
        RetryExecutionResult(core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
    SCP_ERROR_CONTEXT(kGcpAuthTokenFetcher, fetch_token_context, result,
                      "Received JWT could not be parsed into a JSON.");
    FinishContext(result, fetch_token_context, async_executor_);
    return;
  }

  if (!std::all_of(GetRequiredJWTComponents().first,
                   GetRequiredJWTComponents().second,
                   [&json_web_token](const char* const component) {
                     return json_web_token.contains(component);
                   })) {
    auto result =
        RetryExecutionResult(core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN);
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenFetcher, fetch_token_context, result,
        "Received JWT does not contain all the necessary fields.");
    FinishContext(result, fetch_token_context, async_executor_);
    return;
  }

  fetch_token_context.response = make_shared<FetchTokenResponse>();
  fetch_token_context.response->token = http_context.response->body.ToString();

  // We make an assumption that the obtaining token is instantaneous since the
  // token is fetched from the GCP platform close to the VM where this code will
  // run, but in some worst case situations, this token could take longer to
  // obtain and in those cases we will deem the token to be valid for more
  // number of seconds than it is intended to be used for, causing callers to
  // have an expired token for a small time.
  uint64_t expiry_seconds = json_web_token[kJsonTokenExpiryKey].get<uint64_t>();
  uint64_t issued_seconds =
      json_web_token[kJsonTokenIssuedAtKey].get<uint64_t>();
  fetch_token_context.response->token_lifetime_in_seconds =
      seconds(expiry_seconds - issued_seconds);

  FinishContext(SuccessExecutionResult(), fetch_token_context, async_executor_);
}

GcpAuthorizationTokenFetcher::GcpAuthorizationTokenFetcher(
    std::shared_ptr<HttpClientInterface> http_client,
    const std::string& token_target_audience_uri,
    shared_ptr<AsyncExecutorInterface> async_executor)
    : http_client_(http_client),
      token_target_audience_uri_(token_target_audience_uri),
      async_executor_(async_executor),
      host_url_(kTokenServerPath) {}

ExecutionResult GcpAuthorizationTokenFetcher::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpAuthorizationTokenFetcher::FetchToken(
    AsyncContext<FetchTokenRequest, FetchTokenResponse>
        fetch_token_context) noexcept {
  // Make a request to the metadata server:
  // The PBS is running on a GCP VM which runs as a service account. Services
  // which run on GCP also spin up a local metadata server which can be
  // queried for details about the system. curl -H "Metadata-Flavor: Google"
  // 'http://metadata/computeMetadata/v1/instance/service-accounts/default/identity?audience=AUDIENCE'

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.activity_id = fetch_token_context.activity_id;
  http_context.correlation_id = fetch_token_context.correlation_id;
  http_context.request = make_shared<HttpRequest>();

  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {string(kMetadataFlavorHeader), string(kMetadataFlavorHeaderValue)});

  http_context.request->path = make_shared<Uri>(host_url_);
  http_context.request->query = make_shared<string>(
      absl::StrCat(kAudienceParameter, token_target_audience_uri_, "&",
                   kFormatFullParameter));

  http_context.callback =
      bind(&GcpAuthorizationTokenFetcher::ProcessHttpResponse, this,
           fetch_token_context, _1);

  return http_client_->PerformRequest(http_context);
}

}  // namespace google::scp::pbs
