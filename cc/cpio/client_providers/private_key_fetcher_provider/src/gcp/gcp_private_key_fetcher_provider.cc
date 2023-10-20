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

#include "gcp_private_key_fetcher_provider.h"

#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/interface/http_client_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/private_key_fetcher_provider_utils.h"

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;

namespace {
constexpr char kGcpPrivateKeyFetcherProvider[] = "GcpPrivateKeyFetcherProvider";
constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kBearerTokenPrefix[] = "Bearer ";
}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult GcpPrivateKeyFetcherProvider::Init() noexcept {
  RETURN_IF_FAILURE(PrivateKeyFetcherProvider::Init());

  if (!auth_token_provider_) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND);
    SCP_ERROR(kGcpPrivateKeyFetcherProvider, kZeroUuid, execution_result,
              "Failed to get credentials provider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpPrivateKeyFetcherProvider::SignHttpRequest(
    AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
        sign_request_context) noexcept {
  auto request = make_shared<GetSessionTokenForTargetAudienceRequest>();
  request->token_target_audience_uri = make_shared<string>(
      sign_request_context.request->key_vending_endpoint
          ->gcp_private_key_vending_service_cloudfunction_url);
  AsyncContext<GetSessionTokenForTargetAudienceRequest, GetSessionTokenResponse>
      get_token_context(
          move(request),
          bind(&GcpPrivateKeyFetcherProvider::OnGetSessionTokenCallback, this,
               sign_request_context, _1),
          sign_request_context);

  return auth_token_provider_->GetSessionTokenForTargetAudience(
      get_token_context);
}

void GcpPrivateKeyFetcherProvider::OnGetSessionTokenCallback(
    AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
        sign_request_context,
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpPrivateKeyFetcherProvider, sign_request_context,
        get_token_context.result,
        "Failed to get the access token for audience target %s.",
        get_token_context.request->token_target_audience_uri->c_str());
    sign_request_context.result = get_token_context.result;
    sign_request_context.Finish();
    return;
  }

  const auto& access_token = *get_token_context.response->session_token;
  auto http_request = make_shared<HttpRequest>();
  PrivateKeyFetchingClientUtils::CreateHttpRequest(
      *sign_request_context.request, *http_request);
  http_request->headers = make_shared<core::HttpHeaders>();
  http_request->headers->insert(
      {string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});
  sign_request_context.response = move(http_request);
  sign_request_context.result = SuccessExecutionResult();
  sign_request_context.Finish();
}

#ifndef TEST_CPIO
std::shared_ptr<PrivateKeyFetcherProviderInterface>
PrivateKeyFetcherProviderFactory::Create(
    const shared_ptr<HttpClientInterface>& http_client,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<AuthTokenProviderInterface>& auth_token_provider) {
  return make_shared<GcpPrivateKeyFetcherProvider>(http_client,
                                                   auth_token_provider);
}
#endif
}  // namespace google::scp::cpio::client_providers
