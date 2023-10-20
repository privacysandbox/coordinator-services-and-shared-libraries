// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/auth_token_provider/src/aws/aws_auth_token_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "core/curl_client/mock/mock_curl_client.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/auth_token_provider/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::errors::
    SC_AWS_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::MockCurlClient;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::atomic_bool;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::unique_ptr;
using std::chrono::seconds;
using testing::Eq;
using testing::Pair;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace {

constexpr char kTokenServerPath[] = "http://169.254.169.254/latest/api/token";
constexpr char kTokenTtlInSecondHeader[] =
    "X-aws-ec2-metadata-token-ttl-seconds";
constexpr int kTokenTtlInSecondHeaderValue = 21600;

constexpr char kHttpResponseMock[] =
    "TEST_AQAEACXaJIGChRZqwNuG_2hCfQq73UOSCONaS-25g==";

}  // namespace

namespace google::scp::cpio::client_providers::test {

class AwsAuthTokenProviderTest : public testing::TestWithParam<string> {
 protected:
  AwsAuthTokenProviderTest()
      : http_client_(make_shared<MockCurlClient>()),
        authorizer_provider_(make_unique<AwsAuthTokenProvider>(http_client_)) {}

  string GetResponseBody() { return GetParam(); }

  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      fetch_token_context_;

  shared_ptr<MockCurlClient> http_client_;
  unique_ptr<AwsAuthTokenProvider> authorizer_provider_;
};

TEST_F(AwsAuthTokenProviderTest,
       GetSessionTokenSuccessWithValidTokenAndExpireTime) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = SuccessExecutionResult();
    EXPECT_EQ(http_context.request->method, HttpMethod::PUT);
    EXPECT_THAT(http_context.request->path, Pointee(Eq(kTokenServerPath)));
    EXPECT_THAT(http_context.request->headers,
                Pointee(UnorderedElementsAre(
                    Pair(kTokenTtlInSecondHeader,
                         to_string(kTokenTtlInSecondHeaderValue)))));

    http_context.response = make_shared<HttpResponse>();
    http_context.response->body = BytesBuffer(kHttpResponseMock);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  atomic_bool finished(false);
  fetch_token_context_.callback = [&finished](auto& context) {
    EXPECT_SUCCESS(context.result);
    ASSERT_TRUE(context.response);
    EXPECT_THAT(context.response->session_token,
                Pointee(Eq(kHttpResponseMock)));
    EXPECT_EQ(context.response->token_lifetime_in_seconds,
              seconds(kTokenTtlInSecondHeaderValue));
    finished = true;
  };
  EXPECT_THAT(authorizer_provider_->GetSessionToken(fetch_token_context_),
              IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_F(AwsAuthTokenProviderTest, GetSessionTokenFailsIfHttpRequestFails) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = FailureExecutionResult(SC_UNKNOWN);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  atomic_bool finished(false);
  fetch_token_context_.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
    finished = true;
  };
  EXPECT_THAT(authorizer_provider_->GetSessionToken(fetch_token_context_),
              IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_F(AwsAuthTokenProviderTest, NullHttpClientProvider) {
  auto auth_token_provider = make_shared<AwsAuthTokenProvider>(nullptr);

  EXPECT_THAT(auth_token_provider->Init(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED)));
}

}  // namespace google::scp::cpio::client_providers::test
