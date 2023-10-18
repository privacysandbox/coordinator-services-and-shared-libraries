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

#include "aws_auth_token_provider.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "cc/core/common/uuid/src/uuid.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED;
using std::bind;
using std::make_shared;
using std::move;
using std::pair;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::chrono::seconds;
using std::placeholders::_1;

namespace {
constexpr char kAwsAuthTokenProvider[] = "AwsAuthTokenProvider";

/// Use IMDSv2. The IPv4 address of the IMDSv2 is 169.254.169.254.
/// For more information, see
/// https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html
constexpr char kTokenServerPath[] = "http://169.254.169.254/latest/api/token";
constexpr char kTokenTtlInSecondHeader[] =
    "X-aws-ec2-metadata-token-ttl-seconds";
constexpr int kTokenTtlInSecondHeaderValue = 21600;

}  // namespace

namespace google::scp::cpio::client_providers {
AwsAuthTokenProvider::AwsAuthTokenProvider(
    const shared_ptr<HttpClientInterface>& http_client)
    : http_client_(http_client) {}

ExecutionResult AwsAuthTokenProvider::Init() noexcept {
  if (!http_client_) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kAwsAuthTokenProvider, kZeroUuid, execution_result, ,
              "Http client cannot be nullptr.");
    return execution_result;
  }

  return SuccessExecutionResult();
};

ExecutionResult AwsAuthTokenProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsAuthTokenProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsAuthTokenProvider::GetSessionToken(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->method = HttpMethod::PUT;
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {string(kTokenTtlInSecondHeader),
       to_string(kTokenTtlInSecondHeaderValue)});

  http_context.request->path = make_shared<Uri>(kTokenServerPath);

  http_context.callback = bind(&AwsAuthTokenProvider::OnGetSessionTokenCallback,
                               this, get_token_context, _1);

  auto execution_result = http_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsAuthTokenProvider, get_token_context,
                      execution_result,
                      "Failed to perform http request to fetch access token.");
    get_token_context.result = execution_result;
    get_token_context.Finish();
    return execution_result;
  }

  return SuccessExecutionResult();
}

void AwsAuthTokenProvider::OnGetSessionTokenCallback(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAwsAuthTokenProvider, get_token_context, http_client_context.result,
        "Failed to get access token from Instance Metadata server");
    get_token_context.result = http_client_context.result;
    get_token_context.Finish();

    return;
  }

  get_token_context.response = make_shared<GetSessionTokenResponse>();
  get_token_context.response->session_token =
      make_shared<string>(http_client_context.response->body.ToString());
  get_token_context.response->token_lifetime_in_seconds =
      seconds(kTokenTtlInSecondHeaderValue);
  get_token_context.result = SuccessExecutionResult();
  get_token_context.Finish();
}

ExecutionResult AwsAuthTokenProvider::GetSessionTokenForTargetAudience(
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context) noexcept {
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

std::shared_ptr<AuthTokenProviderInterface> AuthTokenProviderFactory::Create(
    const std::shared_ptr<core::HttpClientInterface>& http1_client) {
  return make_shared<AwsAuthTokenProvider>(http1_client);
}
}  // namespace google::scp::cpio::client_providers
