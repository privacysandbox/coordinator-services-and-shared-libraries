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

#include "core/common/sized_or_timed_bytes_buffer/src/sized_or_timed_bytes_buffer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::SizedOrTimedBytesBuffer;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::function;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::vector;

namespace google::scp::core::common::test {
TEST(SizedOrTimedBytesBufferTest, InitFunction) {
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    struct Request {};

    struct Response {};

    atomic<bool> condition = false;
    auto get_data_size_function = [](AsyncContext<Request, Response>&) {
      return 1;
    };
    auto serialize_function = [](AsyncContext<Request, Response>&, BytesBuffer&,
                                 size_t&) { return SuccessExecutionResult(); };
    auto flush_function =
        [](BytesBuffer&,
           std::function<void(ExecutionResult&)> on_flush_completed) {
          return SuccessExecutionResult();
        };

    MockAsyncExecutor mock_async_executor;
    mock_async_executor.schedule_for_mock =
        [&](const AsyncOperation& work, Timestamp timestamp,
            function<bool()>& cancellation_callback) {
          EXPECT_EQ(timestamp, 1234);
          condition = true;
          return result;
        };

    shared_ptr<AsyncExecutorInterface> async_executor =
        make_shared<MockAsyncExecutor>(move(mock_async_executor));

    auto sized_or_timed_bytes_buffer =
        make_shared<SizedOrTimedBytesBuffer<Request, Response>>(
            async_executor, get_data_size_function, serialize_function,
            flush_function, 1000, 1234);

    EXPECT_THAT(sized_or_timed_bytes_buffer->Init(), ResultIs(result));
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(SizedOrTimedBytesBufferTest, AppendDataWithMaxSize) {
  struct Request {};

  struct Response {};

  auto get_data_size_function = [](AsyncContext<Request, Response>&) {
    return 1;
  };
  auto serialize_function = [](AsyncContext<Request, Response>&, BytesBuffer&,
                               size_t&) { return SuccessExecutionResult(); };
  auto flush_function =
      [](BytesBuffer&,
         std::function<void(ExecutionResult&)> on_flush_completed) {
        return SuccessExecutionResult();
      };

  MockAsyncExecutor mock_async_executor;
  atomic<bool> schedule_condition = false;
  mock_async_executor.schedule_mock = [&](const AsyncOperation& work) {
    schedule_condition = true;
    return SuccessExecutionResult();
  };

  atomic<bool> schedule_for_condition = false;
  mock_async_executor.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        EXPECT_EQ(timestamp, 1234);
        schedule_for_condition = true;
        return SuccessExecutionResult();
      };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(mock_async_executor));

  // When there is no more capacity available
  auto sized_or_timed_bytes_buffer =
      make_shared<SizedOrTimedBytesBuffer<Request, Response>>(
          async_executor, get_data_size_function, serialize_function,
          flush_function, 0, 1234);

  AsyncContext<Request, Response> context;
  EXPECT_THAT(sized_or_timed_bytes_buffer->Append(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_INITIALIZED)));

  sized_or_timed_bytes_buffer->Init();
  WaitUntil([&]() { return schedule_for_condition.load(); });

  EXPECT_THAT(sized_or_timed_bytes_buffer->Append(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_ENOUGH_SPACE)));
  WaitUntil([&]() { return schedule_condition.load(); });
}

TEST(SizedOrTimedBytesBufferTest, AppendDataWithMaxSizeNoCapacityAvaialble) {
  struct Request {};

  struct Response {};

  auto get_data_size_function = [](AsyncContext<Request, Response>&) {
    return 1;
  };
  auto serialize_function = [](AsyncContext<Request, Response>&, BytesBuffer&,
                               size_t&) { return SuccessExecutionResult(); };
  auto flush_function =
      [](BytesBuffer&,
         std::function<void(ExecutionResult&)> on_flush_completed) {
        return SuccessExecutionResult();
      };

  MockAsyncExecutor mock_async_executor;
  atomic<bool> schedule_condition = false;
  mock_async_executor.schedule_mock = [&](const AsyncOperation& work) {
    schedule_condition = true;
    return SuccessExecutionResult();
  };

  atomic<bool> schedule_for_condition = false;
  mock_async_executor.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        EXPECT_EQ(timestamp, 1234);
        schedule_for_condition = true;
        return SuccessExecutionResult();
      };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(mock_async_executor));
  schedule_for_condition = false;
  schedule_condition = true;

  // When there is capacity available
  auto sized_or_timed_bytes_buffer =
      make_shared<SizedOrTimedBytesBuffer<Request, Response>>(
          async_executor, get_data_size_function, serialize_function,
          flush_function, 1000, 1234);

  AsyncContext<Request, Response> context;
  EXPECT_THAT(sized_or_timed_bytes_buffer->Append(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_INITIALIZED)));

  sized_or_timed_bytes_buffer->Init();
  WaitUntil([&]() { return schedule_for_condition.load(); });

  EXPECT_SUCCESS(sized_or_timed_bytes_buffer->Append(context));
}

TEST(SizedOrTimedBytesBufferTest, FlushData) {
  struct Request {};

  struct Response {};

  auto get_data_size_function = [](AsyncContext<Request, Response>&) {
    return 1;
  };

  function<ExecutionResult(AsyncContext<Request, Response>&, BytesBuffer&,
                           size_t&)>
      serialize_function = [](AsyncContext<Request, Response>&, BytesBuffer&,
                              size_t&) { return SuccessExecutionResult(); };

  function<ExecutionResult(BytesBuffer&, function<void(ExecutionResult&)>)>
      flush_function =
          [](BytesBuffer&,
             std::function<void(ExecutionResult&)> on_flush_completed) {
            return SuccessExecutionResult();
          };

  MockAsyncExecutor mock_async_executor;
  atomic<bool> schedule_condition = false;
  mock_async_executor.schedule_mock = [&](const AsyncOperation& work) {
    schedule_condition = true;
    return SuccessExecutionResult();
  };

  atomic<bool> schedule_for_condition = false;
  mock_async_executor.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        EXPECT_EQ(timestamp, 1234);
        schedule_for_condition = true;
        return SuccessExecutionResult();
      };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(mock_async_executor));
  // When flush then append happens
  auto sized_or_timed_bytes_buffer =
      make_shared<SizedOrTimedBytesBuffer<Request, Response>>(
          async_executor, get_data_size_function, serialize_function,
          flush_function, 0, 1234);

  AsyncContext<Request, Response> context;
  EXPECT_THAT(sized_or_timed_bytes_buffer->Append(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_INITIALIZED)));

  sized_or_timed_bytes_buffer->Init();
  WaitUntil([&]() { return schedule_for_condition.load(); });

  sized_or_timed_bytes_buffer->Flush();

  EXPECT_THAT(sized_or_timed_bytes_buffer->Append(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_BUFFER_IS_SEALED)));
}

TEST(SizedOrTimedBytesBufferTest, AppendBeforeFlushData) {
  struct Request {};

  struct Response {};

  auto get_data_size_function = [](AsyncContext<Request, Response>&) {
    return 1;
  };

  function<ExecutionResult(AsyncContext<Request, Response>&, BytesBuffer&,
                           size_t&)>
      serialize_function = [](AsyncContext<Request, Response>&, BytesBuffer&,
                              size_t&) { return SuccessExecutionResult(); };

  function<ExecutionResult(BytesBuffer&, function<void(ExecutionResult&)>)>
      flush_function =
          [](BytesBuffer&,
             std::function<void(ExecutionResult&)> on_flush_completed) {
            return SuccessExecutionResult();
          };

  MockAsyncExecutor mock_async_executor;
  atomic<bool> schedule_condition = false;
  mock_async_executor.schedule_mock = [&](const AsyncOperation& work) {
    schedule_condition = true;
    return SuccessExecutionResult();
  };

  atomic<bool> schedule_for_condition = false;
  mock_async_executor.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        EXPECT_EQ(timestamp, 1234);
        schedule_for_condition = true;
        return SuccessExecutionResult();
      };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(mock_async_executor));
  // flush callback should be not called if any of the serialization functions
  // fail.
  auto failed_serialize_function = [](AsyncContext<Request, Response>&,
                                      BytesBuffer&, size_t&) {
    return FailureExecutionResult(1234);
  };

  auto sized_or_timed_bytes_buffer =
      make_shared<SizedOrTimedBytesBuffer<Request, Response>>(
          async_executor, get_data_size_function, failed_serialize_function,
          flush_function, 1000, 1234);

  AsyncContext<Request, Response> context;
  atomic<bool> condition = false;
  context.callback = [&](AsyncContext<Request, Response>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
    condition = true;
  };

  sized_or_timed_bytes_buffer->Init();
  WaitUntil([&]() { return schedule_for_condition.load(); });
  sized_or_timed_bytes_buffer->Append(context);
  sized_or_timed_bytes_buffer->Append(context);

  sized_or_timed_bytes_buffer->Flush();
  WaitUntil([&]() { return condition.load(); });
}

TEST(SizedOrTimedBytesBufferTest, FlushFailure) {
  struct Request {};

  struct Response {};

  auto get_data_size_function = [](AsyncContext<Request, Response>&) {
    return 1;
  };

  function<ExecutionResult(AsyncContext<Request, Response>&, BytesBuffer&,
                           size_t&)>
      serialize_function = [](AsyncContext<Request, Response>&, BytesBuffer&,
                              size_t&) { return SuccessExecutionResult(); };

  function<ExecutionResult(BytesBuffer&, function<void(ExecutionResult&)>)>
      flush_function =
          [](BytesBuffer&,
             std::function<void(ExecutionResult&)> on_flush_completed) {
            return SuccessExecutionResult();
          };

  MockAsyncExecutor mock_async_executor;
  atomic<bool> schedule_condition = false;
  mock_async_executor.schedule_mock = [&](const AsyncOperation& work) {
    schedule_condition = true;
    return SuccessExecutionResult();
  };

  atomic<bool> schedule_for_condition = false;
  mock_async_executor.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        EXPECT_EQ(timestamp, 1234);
        schedule_for_condition = true;
        return SuccessExecutionResult();
      };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(mock_async_executor));
  // flush fails, all the waiters should know.
  auto failed_flush_function =
      [](BytesBuffer&,
         std::function<void(ExecutionResult&)> on_flush_completed) mutable {
        return FailureExecutionResult(1234);
      };

  auto sized_or_timed_bytes_buffer =
      make_shared<SizedOrTimedBytesBuffer<Request, Response>>(
          async_executor, get_data_size_function, serialize_function,
          failed_flush_function, 1000, 1234);

  AsyncContext<Request, Response> context;
  atomic<bool> condition = false;
  context.callback = [&](AsyncContext<Request, Response>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
    condition = true;
  };

  sized_or_timed_bytes_buffer->Init();
  WaitUntil([&]() { return schedule_for_condition.load(); });
  sized_or_timed_bytes_buffer->Append(context);
  sized_or_timed_bytes_buffer->Append(context);

  sized_or_timed_bytes_buffer->Flush();
  WaitUntil([&]() { return condition.load(); });
}

TEST(SizedOrTimedBytesBufferTest, ProperFlushData) {
  struct Request {};

  struct Response {};

  auto get_data_size_function = [](AsyncContext<Request, Response>&) {
    return 1;
  };

  function<ExecutionResult(AsyncContext<Request, Response>&, BytesBuffer&,
                           size_t&)>
      serialize_function = [](AsyncContext<Request, Response>&, BytesBuffer&,
                              size_t&) { return SuccessExecutionResult(); };

  function<ExecutionResult(BytesBuffer&, function<void(ExecutionResult&)>)>
      flush_function =
          [](BytesBuffer& bytes_buffer,
             std::function<void(ExecutionResult&)> on_flush_completed) {
            EXPECT_NE(bytes_buffer.length, 0);
            return SuccessExecutionResult();
          };

  MockAsyncExecutor mock_async_executor;
  atomic<bool> schedule_condition = false;
  mock_async_executor.schedule_mock = [&](const AsyncOperation& work) {
    schedule_condition = true;
    return SuccessExecutionResult();
  };

  atomic<bool> schedule_for_condition = false;
  mock_async_executor.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        EXPECT_EQ(timestamp, 1234);
        schedule_for_condition = true;
        return SuccessExecutionResult();
      };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(mock_async_executor));
  // flush callback, all the waiters should know.
  auto failed_flush_function =
      [](BytesBuffer&,
         std::function<void(ExecutionResult&)> on_flush_completed) mutable {
        auto execution_result = FailureExecutionResult(1234);
        on_flush_completed(execution_result);
        return SuccessExecutionResult();
      };

  auto sized_or_timed_bytes_buffer =
      make_shared<SizedOrTimedBytesBuffer<Request, Response>>(
          async_executor, get_data_size_function, serialize_function,
          failed_flush_function, 1000, 1234);

  AsyncContext<Request, Response> context;
  atomic<bool> condition = false;
  context.callback = [&](AsyncContext<Request, Response>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
    condition = true;
  };

  sized_or_timed_bytes_buffer->Init();
  WaitUntil([&]() { return schedule_for_condition.load(); });
  sized_or_timed_bytes_buffer->Append(context);
  sized_or_timed_bytes_buffer->Append(context);

  sized_or_timed_bytes_buffer->Flush();
  WaitUntil([&]() { return condition.load(); });
}

}  // namespace google::scp::core::common::test
