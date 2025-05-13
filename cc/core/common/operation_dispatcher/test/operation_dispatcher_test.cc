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

#include "cc/core/common/operation_dispatcher/src/operation_dispatcher.h"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "cc/core/async_executor/mock/mock_async_executor.h"
#include "cc/core/common/operation_dispatcher/src/error_codes.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/test/utils/conditional_wait.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {
namespace {

TEST(OperationDispatcherTests, SuccessfulOperation) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      std::make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  std::atomic<bool> condition(false);
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_SUCCESS(context.result);
    condition = true;
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, FailedOperation) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      std::make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  std::atomic<bool> condition(false);
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1)));
    condition = true;
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            context.result = FailureExecutionResult(1);
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, RetryOperation) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      std::make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  std::atomic<bool> condition(false);
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_DISPATCHER_EXHAUSTED_RETRIES)));
    EXPECT_EQ(context.retry_count, 5);
    condition = true;
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            context.result = RetryExecutionResult(1);
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, OperationExpiration) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      std::make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  std::atomic<bool> condition(false);
  AsyncContext<std::string, std::string> context;
  context.expiration_time = UINT64_MAX;

  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_DISPATCHER_OPERATION_EXPIRED)));
    EXPECT_EQ(context.retry_count, 4);
    condition = true;
  };

  std::atomic<size_t> retry_count = 0;
  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [&](AsyncContext<std::string, std::string>& context) {
            if (++retry_count == 4) {
              context.expiration_time = 1234;
            }
            context.result = RetryExecutionResult(1);
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, FailedOnAcceptance) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      std::make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  std::atomic<bool> condition(false);
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
    condition = true;
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            return FailureExecutionResult(1234);
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, RetryOnAcceptance) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      std::make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  std::atomic<bool> condition(false);
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_DISPATCHER_EXHAUSTED_RETRIES)));
    condition = true;
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            return RetryExecutionResult(1234);
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace
}  // namespace privacy_sandbox::pbs_common
