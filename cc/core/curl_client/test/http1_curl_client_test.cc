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
#include "core/curl_client/src/http1_curl_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/async_executor/src/async_executor.h"
#include "core/curl_client/src/error_codes.h"
#include "core/curl_client/src/http1_curl_wrapper.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using std::atomic_bool;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::AtLeast;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::IsSupersetOf;
using testing::NiceMock;
using testing::Not;
using testing::Pair;
using testing::Pointee;
using testing::Return;

namespace google::scp::core::test {
namespace {

class MockCurlWrapperProvider : public NiceMock<Http1CurlWrapperProvider> {
 public:
  MOCK_METHOD(ExecutionResultOr<shared_ptr<Http1CurlWrapper>>, MakeWrapper, (),
              (override));
};

class MockCurlWrapper : public NiceMock<Http1CurlWrapper> {
 public:
  MOCK_METHOD(ExecutionResultOr<HttpResponse>, PerformRequest,
              (const HttpRequest&), (override));
};

class Http1CurlClientTest : public ::testing::Test {
 protected:
  Http1CurlClientTest()
      : cpu_async_executor_(
            make_shared<AsyncExecutor>(/*thread_count=*/4, /*queue_cap=*/10)),
        io_async_executor_(
            make_shared<AsyncExecutor>(/*thread_count=*/4, /*queue_cap=*/10)),
        wrapper_(make_shared<MockCurlWrapper>()),
        provider_(make_shared<MockCurlWrapperProvider>()),
        subject_(cpu_async_executor_, io_async_executor_, provider_,
                 common::RetryStrategyOptions(
                     common::RetryStrategyType::Exponential,
                     /*time_duration_ms=*/1UL, /*total_retries=*/10)) {
    assert(cpu_async_executor_->Init().Successful());
    assert(io_async_executor_->Init().Successful());
    assert(cpu_async_executor_->Run().Successful());
    assert(io_async_executor_->Run().Successful());
    ON_CALL(*provider_, MakeWrapper).WillByDefault(Return(wrapper_));
  }

  ~Http1CurlClientTest() {
    assert(io_async_executor_->Stop().Successful());
    assert(cpu_async_executor_->Stop().Successful());
  }

  shared_ptr<AsyncExecutorInterface> cpu_async_executor_, io_async_executor_;

  shared_ptr<MockCurlWrapper> wrapper_;
  shared_ptr<MockCurlWrapperProvider> provider_;

  Http1CurlClient subject_;
};

// We only compare the body but we can add more checks if we want.
MATCHER_P(RequestEquals, expected, "") {
  return ExplainMatchResult(arg.body.ToString(), expected.body.ToString(),
                            result_listener);
}

// We only compare the body but we can add more checks if we want.
MATCHER_P(ResponseEquals, expected, "") {
  return ExplainMatchResult(arg.body.ToString(), expected.body.ToString(),
                            result_listener);
}

TEST_F(Http1CurlClientTest, IssuesPerformRequestOnWrapper) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->body = BytesBuffer("buf");

  HttpRequest expected_request;
  expected_request.body = BytesBuffer("buf");
  HttpResponse response;
  response.body = BytesBuffer("resp");
  EXPECT_CALL(*wrapper_, PerformRequest(RequestEquals(expected_request)))
      .WillOnce(Return(response));

  atomic_bool finished(false);
  http_context.callback = [&response, &finished](auto& http_context) {
    EXPECT_SUCCESS(http_context.result);
    EXPECT_THAT(http_context.response, Pointee(ResponseEquals(response)));
    finished = true;
  };

  ASSERT_THAT(subject_.PerformRequest(http_context), IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_F(Http1CurlClientTest, RetriesWork) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->body = BytesBuffer("buf");

  HttpRequest expected_request;
  expected_request.body = BytesBuffer("buf");
  HttpResponse response;
  response.body = BytesBuffer("resp");

  // Fail 3 times, then succeed.
  {
    InSequence seq;
    EXPECT_CALL(*wrapper_, PerformRequest)
        .Times(3)
        .WillRepeatedly(Return(
            RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FAILED)));
    EXPECT_CALL(*wrapper_, PerformRequest(RequestEquals(expected_request)))
        .WillOnce(Return(response));
  }

  atomic_bool finished(false);
  http_context.callback = [&response, &finished](auto& http_context) {
    EXPECT_SUCCESS(http_context.result);
    EXPECT_THAT(http_context.response, Pointee(ResponseEquals(response)));
    finished = true;
  };

  ASSERT_THAT(subject_.PerformRequest(http_context), IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_F(Http1CurlClientTest, FailureEnds) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->body = BytesBuffer("buf");

  HttpRequest expected_request;
  expected_request.body = BytesBuffer("buf");
  HttpResponse response;
  response.body = BytesBuffer("resp");

  EXPECT_CALL(*wrapper_, PerformRequest)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FAILED)));

  atomic_bool finished(false);
  http_context.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result, Not(IsSuccessful()));
    finished = true;
  };

  ASSERT_THAT(subject_.PerformRequest(http_context), IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

}  // namespace
}  // namespace google::scp::core::test
