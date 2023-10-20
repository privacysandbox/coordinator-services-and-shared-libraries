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

#include "core/token_provider_cache/src/auto_refresh_token_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

#include "core/async_executor/src/async_executor.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/token_fetcher_interface.h"
#include "core/test/utils/conditional_wait.h"
#include "core/token_provider_cache/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Return;

namespace google::scp::core {

class TokenFetcherMock : public TokenFetcherInterface {
 public:
  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, FetchToken,
              ((AsyncContext<FetchTokenRequest, FetchTokenResponse>)),
              (noexcept, override));
};

class AutoRefreshTokenProviderTest : public testing::Test {
 protected:
  AutoRefreshTokenProviderTest()
      : async_executor_(
            make_shared<AsyncExecutor>(2, 1000, true /* drop tasks */)),
        token_fetcher_(nullptr) {
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());

    auto token_fetcher_mock = make_unique<TokenFetcherMock>();
    // Explicit fetching of raw pointer for test. In prod, this will not be done
    // since token fetcher is going to be owned & used by a single component.
    token_fetcher_ = token_fetcher_mock.get();
    token_provider_ = make_unique<AutoRefreshTokenProviderService>(
        move(token_fetcher_mock), async_executor_);
  }

  void TearDown() override { EXPECT_SUCCESS(async_executor_->Stop()); }

  shared_ptr<AsyncExecutor> async_executor_;
  TokenFetcherMock* token_fetcher_;
  unique_ptr<AutoRefreshTokenProviderService> token_provider_;
};

TEST_F(AutoRefreshTokenProviderTest, StartStopWithTokenFailure) {
  EXPECT_CALL(*token_fetcher_, FetchToken)
      .WillRepeatedly(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.result = FailureExecutionResult(1234);
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_THAT(token_provider_->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(token_provider_->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(AutoRefreshTokenProviderTest, StartStopWithTokenSuccess) {
  EXPECT_CALL(*token_fetcher_, FetchToken)
      .WillRepeatedly(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds =
                seconds(1);  // 1 second expiry
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_THAT(token_provider_->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(token_provider_->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(AutoRefreshTokenProviderTest, TokenIsInvalidToStart) {
  shared_ptr<Token> token;
  EXPECT_THAT(token_provider_->GetToken(),
              ResultIs(FailureExecutionResult(
                  errors::SC_AUTO_REFRESH_TOKEN_PROVIDER_TOKEN_NOT_AVAILABLE)));
}

TEST_F(AutoRefreshTokenProviderTest, RunStartsTokenRefresh) {
  atomic<bool> called(false);
  EXPECT_CALL(*token_fetcher_, FetchToken)
      .WillOnce(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds = seconds(1234567);
            called = true;
            return SuccessExecutionResult();
          });

  EXPECT_THAT(token_provider_->Run(), ResultIs(SuccessExecutionResult()));
  test::WaitUntil([&]() { return called.load(); });
}

TEST_F(AutoRefreshTokenProviderTest, QueryFailureRetriesRefresh) {
  atomic<bool> called(false);
  EXPECT_CALL(*token_fetcher_, FetchToken)
      .WillOnce(
          [](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds = seconds(1234567);
            return FailureExecutionResult(1234);
          })
      .WillOnce(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds = seconds(1234567);
            called = true;
            return SuccessExecutionResult();
          });
  EXPECT_THAT(token_provider_->Run(), ResultIs(SuccessExecutionResult()));
  test::WaitUntil([&]() { return called.load(); });
}

TEST_F(AutoRefreshTokenProviderTest, QueryCallbackFailureRetriesRefresh) {
  atomic<bool> called(false);
  EXPECT_CALL(*token_fetcher_, FetchToken)
      .WillOnce(
          [](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds = seconds(1234567);
            context.result = FailureExecutionResult(1234);
            context.Finish();
            return SuccessExecutionResult();
          })
      .WillOnce(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds = seconds(1234567);
            called = true;
            return SuccessExecutionResult();
          });
  EXPECT_THAT(token_provider_->Run(), ResultIs(SuccessExecutionResult()));
  test::WaitUntil([&]() { return called.load(); });
}

TEST_F(AutoRefreshTokenProviderTest, TokenIsCachedAndRefreshed) {
  EXPECT_CALL(*token_fetcher_, FetchToken)
      .WillOnce(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds =
                seconds(1);  // 1 second expiry
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          })
      .WillOnce(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "def";
            context.response->token_lifetime_in_seconds = seconds(360);
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });

  EXPECT_THAT(token_provider_->Run(), ResultIs(SuccessExecutionResult()));

  ExecutionResultOr<shared_ptr<Token>> token_or;
  do {
    token_or = token_provider_->GetToken();
    sleep_for(milliseconds(100));
  } while (!token_or.Successful());

  EXPECT_THAT(token_or, IsSuccessfulAndHolds(Pointee(Eq("abc"))));

  sleep_for(milliseconds(500));

  do {
    token_or = token_provider_->GetToken();
    sleep_for(milliseconds(100));
  } while (token_or.Successful() && *token_or.value() == "abc");

  EXPECT_THAT(token_or, IsSuccessfulAndHolds(Pointee(Eq("def"))));
  EXPECT_THAT(token_provider_->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(AutoRefreshTokenProviderTest, TokenIsCachedAndResetIfTokenFetchFails) {
  EXPECT_CALL(*token_fetcher_, FetchToken)
      .WillOnce(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.response = make_shared<FetchTokenResponse>();
            context.response->token = "abc";
            context.response->token_lifetime_in_seconds =
                seconds(1);  // 1 second expiry
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          })
      .WillRepeatedly(
          [&](AsyncContext<FetchTokenRequest, FetchTokenResponse> context) {
            context.result = FailureExecutionResult(1234);
            context.Finish();
            return SuccessExecutionResult();
          });

  EXPECT_THAT(token_provider_->Run(), ResultIs(SuccessExecutionResult()));

  ExecutionResultOr<shared_ptr<Token>> token_or;
  do {
    token_or = token_provider_->GetToken();
    sleep_for(milliseconds(100));
  } while (!token_or.Successful());

  EXPECT_THAT(token_or, IsSuccessfulAndHolds(Pointee(Eq("abc"))));

  sleep_for(milliseconds(1000));

  EXPECT_THAT(token_provider_->GetToken(),
              ResultIs(FailureExecutionResult(
                  errors::SC_AUTO_REFRESH_TOKEN_PROVIDER_TOKEN_NOT_AVAILABLE)));

  EXPECT_THAT(token_provider_->Stop(), ResultIs(SuccessExecutionResult()));
}
}  // namespace google::scp::core
