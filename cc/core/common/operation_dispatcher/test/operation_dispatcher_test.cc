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

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/common/operation_dispatcher/src/error_codes.h"
#include "core/interface/async_context.h"
#include "core/interface/streaming_context.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::function;
using std::make_shared;
using std::string;
using std::chrono::milliseconds;

namespace google::scp::core::common::test {
TEST(OperationDispatcherTests, SuccessfulOperation) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  AsyncContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_SUCCESS(context.result);
    condition = true;
  };

  function<ExecutionResult(AsyncContext<string, string>&)>
      dispatch_to_component = [](AsyncContext<string, string>& context) {
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, SuccessfulOperationProducerStreaming) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  ProducerStreamingContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_SUCCESS(context.result);
    condition = true;
  };

  function<ExecutionResult(ProducerStreamingContext<string, string>&)>
      dispatch_to_component =
          [](ProducerStreamingContext<string, string>& context) {
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.DispatchProducerStreaming(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, SuccessfulOperationConsumerStreaming) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<int> process_call_count(0);
  atomic<bool> condition(false);
  ConsumerStreamingContext<string, string> context;
  context.process_callback =
      [&](ConsumerStreamingContext<string, string>& context, bool is_finish) {
        if (is_finish) {
          EXPECT_SUCCESS(context.result);
          condition = true;
        } else {
          process_call_count++;
        }
      };

  function<ExecutionResult(ConsumerStreamingContext<string, string>&)>
      dispatch_to_component =
          [](ConsumerStreamingContext<string, string>& context) {
            context.ProcessNextMessage();
            context.ProcessNextMessage();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.DispatchConsumerStreaming(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
  // Expect it to be called twice - once per ProcessNextMessage call.
  EXPECT_EQ(process_call_count, 2);
}

TEST(OperationDispatcherTests, FailedOperation) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  AsyncContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1)));
    condition = true;
  };

  function<ExecutionResult(AsyncContext<string, string>&)>
      dispatch_to_component = [](AsyncContext<string, string>& context) {
        context.result = FailureExecutionResult(1);
        context.Finish();
        return SuccessExecutionResult();
      };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, FailedOperationProducerStreaming) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  ProducerStreamingContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1)));
    condition = true;
  };

  function<ExecutionResult(ProducerStreamingContext<string, string>&)>
      dispatch_to_component =
          [](ProducerStreamingContext<string, string>& context) {
            context.result = FailureExecutionResult(1);
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.DispatchProducerStreaming(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, FailedOperationConsumerStreaming) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<int> process_call_count(0);
  atomic<bool> condition(false);
  ConsumerStreamingContext<string, string> context;
  context.process_callback =
      [&](ConsumerStreamingContext<string, string>& context, bool is_finish) {
        if (is_finish) {
          EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1)));
          condition = true;
        } else {
          process_call_count++;
        }
      };

  function<ExecutionResult(ConsumerStreamingContext<string, string>&)>
      dispatch_to_component =
          [](ConsumerStreamingContext<string, string>& context) {
            context.ProcessNextMessage();
            context.ProcessNextMessage();
            context.result = FailureExecutionResult(1);
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.DispatchConsumerStreaming(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
  // Expect it to be called twice - once per ProcessNextMessage call.
  EXPECT_EQ(process_call_count, 2);
}

TEST(OperationDispatcherTests, RetryOperation) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  AsyncContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    EXPECT_EQ(context.retry_count, 5);
    condition = true;
  };

  function<ExecutionResult(AsyncContext<string, string>&)>
      dispatch_to_component = [](AsyncContext<string, string>& context) {
        context.result = RetryExecutionResult(1);
        context.Finish();
        return SuccessExecutionResult();
      };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, RetryOperationProducerStreaming) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  ProducerStreamingContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    EXPECT_EQ(context.retry_count, 5);
    condition = true;
  };

  function<ExecutionResult(ProducerStreamingContext<string, string>&)>
      dispatch_to_component =
          [](ProducerStreamingContext<string, string>& context) {
            context.result = RetryExecutionResult(1);
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.DispatchProducerStreaming(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, RetryOperationConsumerStreaming) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<int> process_call_count(0);
  atomic<bool> condition(false);
  ConsumerStreamingContext<string, string> context;
  context.process_callback =
      [&](ConsumerStreamingContext<string, string>& context, bool is_finish) {
        if (is_finish) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
          EXPECT_EQ(context.retry_count, 5);
          condition = true;
        } else {
          process_call_count++;
        }
      };

  function<ExecutionResult(ConsumerStreamingContext<string, string>&)>
      dispatch_to_component =
          [](ConsumerStreamingContext<string, string>& context) {
            context.ProcessNextMessage();
            context.ProcessNextMessage();
            context.result = RetryExecutionResult(1);
            context.Finish();
            return SuccessExecutionResult();
          };

  dispatcher.DispatchConsumerStreaming(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
  // Expect 2 calls per try.
  int expected_call_count = 2 * retry_strategy.GetMaximumAllowedRetryCount();
  EXPECT_EQ(process_call_count, expected_call_count);
}

TEST(OperationDispatcherTests, OperationExpiration) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  AsyncContext<string, string> context;
  context.expiration_time = UINT64_MAX;

  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_OPERATION_EXPIRED)));
    EXPECT_EQ(context.retry_count, 4);
    condition = true;
  };

  atomic<size_t> retry_count = 0;
  function<ExecutionResult(AsyncContext<string, string>&)>
      dispatch_to_component = [&](AsyncContext<string, string>& context) {
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
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  AsyncContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
    condition = true;
  };

  function<ExecutionResult(AsyncContext<string, string>&)>
      dispatch_to_component = [](AsyncContext<string, string>& context) {
        return FailureExecutionResult(1234);
      };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

TEST(OperationDispatcherTests, RetryOnAcceptance) {
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(mock_async_executor, retry_strategy);

  atomic<bool> condition(false);
  AsyncContext<string, string> context;
  context.callback = [&](AsyncContext<string, string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    condition = true;
  };

  function<ExecutionResult(AsyncContext<string, string>&)>
      dispatch_to_component = [](AsyncContext<string, string>& context) {
        return RetryExecutionResult(1234);
      };

  dispatcher.Dispatch(context, dispatch_to_component);
  WaitUntil([&]() { return condition.load(); });
}

}  // namespace google::scp::core::common::test
