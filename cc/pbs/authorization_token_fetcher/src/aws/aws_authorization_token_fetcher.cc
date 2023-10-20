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
#include "aws_authorization_token_fetcher.h"

#include "core/authorization_service/src/aws_authorizer_client_signer.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/http2_client/src/aws/aws_v4_signer.h"

using google::scp::core::AsyncContext;
using google::scp::core::AwsAuthorizerClientSigner;
using google::scp::core::ExecutionResult;
using google::scp::core::FetchTokenRequest;
using google::scp::core::FetchTokenResponse;
using google::scp::core::GetCredentialsRequest;
using google::scp::core::GetCredentialsResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::TimeProvider;
using std::make_shared;
using std::string;
using std::chrono::seconds;
using std::placeholders::_1;

static constexpr size_t kTokenValidityInSeconds = 100;

namespace google::scp::pbs {

ExecutionResult AwsAuthorizationTokenFetcher::Init() noexcept {
  RETURN_IF_FAILURE(credentials_provider_->Init());
  return SuccessExecutionResult();
}

ExecutionResult AwsAuthorizationTokenFetcher::FetchToken(
    AsyncContext<FetchTokenRequest, FetchTokenResponse>
        token_request_context) noexcept {
  AsyncContext<GetCredentialsRequest, GetCredentialsResponse>
      get_credentials_context(
          make_shared<GetCredentialsRequest>(),
          bind(&AwsAuthorizationTokenFetcher::OnGetCredentialsCallback, this,
               token_request_context, _1),
          token_request_context);
  return credentials_provider_->GetCredentials(get_credentials_context);
}

void AwsAuthorizationTokenFetcher::OnGetCredentialsCallback(
    AsyncContext<FetchTokenRequest, FetchTokenResponse> token_request_context,
    AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
        get_credentials_context) noexcept {
  if (!get_credentials_context.result.Successful()) {
    token_request_context.result = get_credentials_context.result;
    token_request_context.Finish();
    return;
  }

  AwsAuthorizerClientSigner signer(
      *get_credentials_context.response->access_key_id,
      *get_credentials_context.response->access_key_secret,
      *get_credentials_context.response->security_token, region_);

  string auth_token;
  auto execution_result = signer.GetAuthToken(gateway_endpoint_, auth_token);
  if (!execution_result.Successful()) {
    token_request_context.result = execution_result;
    token_request_context.Finish();
    return;
  }

  token_request_context.response = make_shared<FetchTokenResponse>();
  token_request_context.response->token = auth_token;
  token_request_context.response->token_lifetime_in_seconds =
      seconds(kTokenValidityInSeconds);
  token_request_context.result = get_credentials_context.result;
  token_request_context.Finish();
}
}  // namespace google::scp::pbs
