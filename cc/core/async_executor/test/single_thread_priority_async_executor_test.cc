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

#include "core/async_executor/src/single_thread_priority_async_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "core/async_executor/src/async_executor.h"
#include "core/async_executor/src/error_codes.h"
#include "core/async_executor/src/typedef.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/test/test_config.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::common::TimeProvider;
using std::atomic;
using std::function;
using std::make_shared;
using std::string;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;

namespace google::scp::core::test {

TEST(SingleThreadPriorityAsyncExecutorTests, CannotInitWithTooBigQueueCap) {
  SingleThreadPriorityAsyncExecutor executor(kMaxQueueCap + 1);
  EXPECT_EQ(executor.Init(), FailureExecutionResult(
                                 errors::SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP));
}

TEST(SingleThreadPriorityAsyncExecutorTests, EmptyWorkQueue) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_EQ(executor.Init(), SuccessExecutionResult());
  EXPECT_EQ(executor.Run(), SuccessExecutionResult());
  EXPECT_EQ(executor.Stop(), SuccessExecutionResult());
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotRunTwice) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_EQ(executor.Init(), SuccessExecutionResult());
  EXPECT_EQ(executor.Run(), SuccessExecutionResult());
  EXPECT_EQ(executor.Run(),
            FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_ALREADY_RUNNING));
  EXPECT_EQ(executor.Stop(), SuccessExecutionResult());
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotStopTwice) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_EQ(executor.Init(), SuccessExecutionResult());
  EXPECT_EQ(executor.Run(), SuccessExecutionResult());
  EXPECT_EQ(executor.Stop(), SuccessExecutionResult());
  EXPECT_EQ(executor.Stop(),
            FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotScheduleWorkBeforeInit) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_EQ(executor.ScheduleFor([]() {}, 10000),
            FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotScheduleWorkBeforeRun) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_EQ(executor.Init(), SuccessExecutionResult());
  EXPECT_EQ(executor.ScheduleFor([]() {}, 1000),
            FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotRunBeforeInit) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_EQ(executor.Run(),
            FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotStopBeforeRun) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_EQ(executor.Init(), SuccessExecutionResult());
  EXPECT_EQ(executor.Stop(),
            FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING));
}

TEST(SingleThreadPriorityAsyncExecutorTests, ExceedingQueueCapSchedule) {
  int queue_cap = 1;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  AsyncTask task;
  auto two_seconds = duration_cast<nanoseconds>(seconds(2)).count();

  auto schedule_for_timestamp = task.GetExecutionTimestamp() + two_seconds;
  executor.ScheduleFor([&]() {}, schedule_for_timestamp);
  auto result = executor.ScheduleFor([&]() {}, task.GetExecutionTimestamp());
  EXPECT_EQ(result, RetryExecutionResult(
                        errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP));

  executor.Stop();
}

TEST(SingleThreadPriorityAsyncExecutorTests, CountWorkSingleThread) {
  int queue_cap = 10;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  atomic<int> count(0);
  for (int i = 0; i < queue_cap; i++) {
    EXPECT_EQ(executor.ScheduleFor([&]() { count++; }, 123456),
              SuccessExecutionResult());
  }
  // Waits some time to finish the work.
  WaitUntil([&]() { return count == queue_cap; });
  EXPECT_EQ(count, queue_cap);

  executor.Stop();
}

TEST(SingleThreadPriorityAsyncExecutorTests, OrderedTasksExecution) {
  int queue_cap = 10;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  AsyncTask task;
  auto half_second = duration_cast<nanoseconds>(milliseconds(500)).count();
  auto one_second = duration_cast<nanoseconds>(seconds(1)).count();
  auto two_seconds = duration_cast<nanoseconds>(seconds(2)).count();

  atomic<size_t> counter(0);
  executor.ScheduleFor([&]() { EXPECT_EQ(counter++, 2); },
                       task.GetExecutionTimestamp() + two_seconds);
  executor.ScheduleFor([&]() { EXPECT_EQ(counter++, 1); },
                       task.GetExecutionTimestamp() + one_second);
  executor.ScheduleFor([&]() { EXPECT_EQ(counter++, 0); },
                       task.GetExecutionTimestamp() + half_second);

  WaitUntil([&]() { return counter == 3; });
  executor.Stop();
}

TEST(SingleThreadPriorityAsyncExecutorTests, AsyncContextCallback) {
  SingleThreadPriorityAsyncExecutor executor(10);
  executor.Init();
  executor.Run();

  // Atomic is not used here because we just reserve one thread in the
  size_t callback_count = 0;
  auto request = make_shared<string>("request");
  auto callback = [&](AsyncContext<string, string>& context) {
    callback_count++;
  };
  auto context = AsyncContext<string, string>(request, callback);

  executor.ScheduleFor(
      [&]() {
        context.response = make_shared<string>("response");
        context.result = SuccessExecutionResult();
        context.Finish();
      },
      12345);

  // Waits some time to finish the work.
  WaitUntil([&]() { return callback_count == 1; });

  // Verifies the work is executed.
  EXPECT_EQ(*(context.response), "response");
  EXPECT_EQ(context.result, SuccessExecutionResult());
  // Verifies the callback is executed.
  EXPECT_EQ(callback_count, 1);

  executor.Stop();
}

TEST(SingleThreadPriorityAsyncExecutorTests, FinishWorkWhenStopInMiddle) {
  int queue_cap = 5;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  atomic<int> urgent_count(0);
  for (int i = 0; i < queue_cap; i++) {
    executor.ScheduleFor(
        [&]() {
          urgent_count++;
          std::this_thread::sleep_for(UNIT_TEST_SHORT_SLEEP_MS);
        },
        1234);
  }
  executor.Stop();

  // Waits some time to finish the work.
  WaitUntil([&]() { return urgent_count == queue_cap; });

  EXPECT_EQ(urgent_count, queue_cap);
}

TEST(SingleThreadPriorityAsyncExecutorTests, TaskCancellation) {
  int queue_cap = 3;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  for (int i = 0; i < queue_cap; i++) {
    function<bool()> cancellation_callback;
    Timestamp next_clock =
        (TimeProvider::GetSteadyTimestampInNanoseconds() + milliseconds(500))
            .count();

    executor.ScheduleFor([&]() { EXPECT_EQ(true, false); }, next_clock,
                         cancellation_callback);

    EXPECT_EQ(cancellation_callback(), true);
  }
  executor.Stop();

  std::this_thread::sleep_for(seconds(2));
}
}  // namespace google::scp::core::test
