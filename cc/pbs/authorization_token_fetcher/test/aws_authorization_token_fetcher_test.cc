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

#include "pbs/authorization_token_fetcher/src/aws/aws_authorization_token_fetcher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::test::ResultIs;
using std::atomic;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::chrono::seconds;
using ::testing::_;

using google::scp::pbs::AwsAuthorizationTokenFetcher;

namespace google::scp::core {
class CredentialsProviderMock : public CredentialsProviderInterface {
 public:
  MOCK_METHOD(ExecutionResult, GetCredentials,
              ((AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&)),
              (noexcept, override));
  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));
};

class AwsAuthorizationTokenFetcherTest : public testing::Test {
 protected:
  AwsAuthorizationTokenFetcherTest()
      : credentials_provider_mock_(make_shared<CredentialsProviderMock>()),
        auth_token_fetcher_("http://pbs.com/auth", "us-east-1",
                            credentials_provider_mock_) {}

  shared_ptr<CredentialsProviderMock> credentials_provider_mock_;
  AwsAuthorizationTokenFetcher auth_token_fetcher_;
};

TEST_F(AwsAuthorizationTokenFetcherTest,
       FetchTokenGivesValidTokenAndTokenExpirationTimestamp) {
  atomic<bool> callback_invoked(false);

  EXPECT_CALL(*credentials_provider_mock_, GetCredentials)
      .WillOnce([&](AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
                        context) {
        context.response = make_shared<GetCredentialsResponse>();
        context.response->access_key_id =
            make_shared<string>("ATESTHTESTTEST6FTEST");
        context.response->access_key_secret =
            make_shared<string>("TE1testMv1Hkpqtest/testte/TESTtesttestqv");
        context.response->security_token =
            make_shared<string>("qwsafgAJKfgakjFGsa");
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  AsyncContext<FetchTokenRequest, FetchTokenResponse> context;
  context.request = make_shared<FetchTokenRequest>();
  context.callback =
      [&](AsyncContext<FetchTokenRequest, FetchTokenResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_NE(context.response->token, "");
        EXPECT_NE(context.response->token_lifetime_in_seconds, seconds::max());
        callback_invoked = true;
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(auth_token_fetcher_.FetchToken(context));
  test::WaitUntil([&]() { return callback_invoked.load(); });
}

TEST_F(AwsAuthorizationTokenFetcherTest,
       FetchTokenFailsIfCredentialsProviderFails) {
  EXPECT_CALL(*credentials_provider_mock_, GetCredentials)
      .WillOnce([&](AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
                        context) { return FailureExecutionResult(1234); });

  AsyncContext<FetchTokenRequest, FetchTokenResponse> context;
  context.request = make_shared<FetchTokenRequest>();

  EXPECT_EQ(auth_token_fetcher_.FetchToken(context),
            FailureExecutionResult(1234));
}

TEST_F(AwsAuthorizationTokenFetcherTest,
       FetchTokenFailsIfCredentialsProviderResponseFails) {
  atomic<bool> callback_invoked(false);

  EXPECT_CALL(*credentials_provider_mock_, GetCredentials)
      .WillOnce([&](AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
                        context) {
        context.result = FailureExecutionResult(1234);
        context.Finish();
        return SuccessExecutionResult();
      });

  AsyncContext<FetchTokenRequest, FetchTokenResponse> context;
  context.request = make_shared<FetchTokenRequest>();
  context.callback =
      [&](AsyncContext<FetchTokenRequest, FetchTokenResponse>& context) {
        EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
        callback_invoked = true;
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(auth_token_fetcher_.FetchToken(context));
  test::WaitUntil([&]() { return callback_invoked.load(); });
}

}  // namespace google::scp::core
