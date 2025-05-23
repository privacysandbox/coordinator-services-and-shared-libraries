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

#include "cc/core/async_executor/src/single_thread_priority_async_executor.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "cc/core/async_executor/src/error_codes.h"
#include "cc/core/async_executor/src/typedef.h"
#include "cc/core/common/time_provider/src/time_provider.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/test/test_config.h"
#include "cc/core/test/utils/conditional_wait.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {
namespace {

using ::testing::Values;

TEST(SingleThreadPriorityAsyncExecutorTests, CannotInitWithTooBigQueueCap) {
  SingleThreadPriorityAsyncExecutor executor(kMaxQueueCap + 1);
  EXPECT_THAT(
      executor.Init(),
      ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP)));
}

TEST(SingleThreadPriorityAsyncExecutorTests, EmptyWorkQueue) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());
  EXPECT_SUCCESS(executor.Stop());
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotRunTwice) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());
  EXPECT_THAT(
      executor.Run(),
      ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_ALREADY_RUNNING)));
  EXPECT_SUCCESS(executor.Stop());
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotStopTwice) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());
  EXPECT_SUCCESS(executor.Stop());
  EXPECT_THAT(executor.Stop(),
              ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotScheduleWorkBeforeInit) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_THAT(executor.ScheduleFor([]() {}, 10000),
              ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotScheduleWorkBeforeRun) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_THAT(executor.ScheduleFor([]() {}, 1000),
              ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotRunBeforeInit) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_THAT(
      executor.Run(),
      ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_INITIALIZED)));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CannotStopBeforeRun) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_THAT(executor.Stop(),
              ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadPriorityAsyncExecutorTests, ExceedingQueueCapSchedule) {
  int queue_cap = 1;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  AsyncTask task;
  auto two_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::seconds(2))
                         .count();

  auto schedule_for_timestamp = task.GetExecutionTimestamp() + two_seconds;
  EXPECT_SUCCESS(executor.ScheduleFor([&]() {}, schedule_for_timestamp));
  auto result = executor.ScheduleFor([&]() {}, task.GetExecutionTimestamp());
  EXPECT_THAT(
      result,
      ResultIs(RetryExecutionResult(SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP)));

  EXPECT_SUCCESS(executor.Stop());
}

TEST(SingleThreadPriorityAsyncExecutorTests, CountWorkSingleThread) {
  int queue_cap = 10;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  std::atomic<int> count(0);
  for (int i = 0; i < queue_cap; i++) {
    EXPECT_SUCCESS(executor.ScheduleFor([&]() { count++; }, 123456));
  }
  // Waits some time to finish the work.
  WaitUntil([&]() { return count == queue_cap; }, std::chrono::seconds(30));
  EXPECT_EQ(count, queue_cap);

  EXPECT_SUCCESS(executor.Stop());
}

class AffinityTest : public testing::TestWithParam<size_t> {
 protected:
  size_t GetCpu() const { return GetParam(); }
};

TEST_P(AffinityTest, CountWorkSingleThreadWithAffinity) {
  int queue_cap = 10;
  SingleThreadPriorityAsyncExecutor executor(queue_cap, false, GetCpu());
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  std::atomic<int> count(0);
  for (int i = 0; i < queue_cap; i++) {
    EXPECT_SUCCESS(executor.ScheduleFor(
        [&]() {
          cpu_set_t cpuset;
          CPU_ZERO(&cpuset);
          pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
          if (GetCpu() < std::thread::hardware_concurrency()) {
            EXPECT_NE(CPU_ISSET(GetCpu(), &cpuset), 0);
          }
          count++;
        },
        123456));
  }
  // Waits some time to finish the work.
  WaitUntil([&]() { return count == queue_cap; });
  EXPECT_EQ(count, queue_cap);

  EXPECT_SUCCESS(executor.Stop());
}

// The test should work for any value, even an invalid CPU #.
INSTANTIATE_TEST_SUITE_P(SingleThreadPriorityAsyncExecutorTests, AffinityTest,
                         Values(0, 1, std::thread::hardware_concurrency() - 1,
                                std::thread::hardware_concurrency()));

TEST(SingleThreadPriorityAsyncExecutorTests, OrderedTasksExecution) {
  int queue_cap = 10;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  AsyncTask task;
  auto half_second = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::milliseconds(500))
                         .count();
  auto one_second = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::seconds(1))
                        .count();
  auto two_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::seconds(2))
                         .count();

  std::atomic<size_t> counter(0);
  EXPECT_SUCCESS(
      executor.ScheduleFor([&]() { EXPECT_EQ(counter++, 2); },
                           task.GetExecutionTimestamp() + two_seconds));
  EXPECT_SUCCESS(
      executor.ScheduleFor([&]() { EXPECT_EQ(counter++, 1); },
                           task.GetExecutionTimestamp() + one_second));
  EXPECT_SUCCESS(
      executor.ScheduleFor([&]() { EXPECT_EQ(counter++, 0); },
                           task.GetExecutionTimestamp() + half_second));

  WaitUntil([&]() { return counter == 3; }, std::chrono::seconds(30));
  EXPECT_SUCCESS(executor.Stop());
}

TEST(SingleThreadPriorityAsyncExecutorTests, AsyncContextCallback) {
  SingleThreadPriorityAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  // Atomic is not used here because we just reserve one thread in the
  size_t callback_count = 0;
  auto request = std::make_shared<std::string>("request");
  auto callback = [&](AsyncContext<std::string, std::string>& context) {
    callback_count++;
  };
  auto context = AsyncContext<std::string, std::string>(request, callback);

  EXPECT_SUCCESS(executor.ScheduleFor(
      [&]() {
        context.response = std::make_shared<std::string>("response");
        context.result = SuccessExecutionResult();
        context.Finish();
      },
      12345));

  // Waits some time to finish the work.
  WaitUntil([&]() { return callback_count == 1; }, std::chrono::seconds(30));

  // Verifies the work is executed.
  EXPECT_EQ(*(context.response), "response");
  EXPECT_SUCCESS(context.result);
  // Verifies the callback is executed.
  EXPECT_EQ(callback_count, 1);

  EXPECT_SUCCESS(executor.Stop());
}

TEST(SingleThreadPriorityAsyncExecutorTests, FinishWorkWhenStopInMiddle) {
  int queue_cap = 5;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  std::atomic<int> urgent_count(0);
  for (int i = 0; i < queue_cap; i++) {
    EXPECT_SUCCESS(executor.ScheduleFor(
        [&]() {
          urgent_count++;
          std::this_thread::sleep_for(UNIT_TEST_SHORT_SLEEP_MS);
        },
        1234));
  }
  EXPECT_SUCCESS(executor.Stop());

  // Waits some time to finish the work.
  WaitUntil([&]() { return urgent_count == queue_cap; },
            std::chrono::seconds(30));

  EXPECT_EQ(urgent_count, queue_cap);
}

TEST(SingleThreadPriorityAsyncExecutorTests, TaskCancellation) {
  int queue_cap = 3;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  for (int i = 0; i < queue_cap; i++) {
    std::function<bool()> cancellation_callback;
    Timestamp next_clock = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                            std::chrono::milliseconds(500))
                               .count();

    EXPECT_SUCCESS(executor.ScheduleFor([&]() { EXPECT_EQ(true, false); },
                                        next_clock, cancellation_callback));

    EXPECT_EQ(cancellation_callback(), true);
  }
  EXPECT_SUCCESS(executor.Stop());

  std::this_thread::sleep_for(std::chrono::seconds(2));
}

TEST(SingleThreadPriorityAsyncExecutorTests,
     DuringStopDoNotWaitOnCancelledTaskExecutionTimeToArrive) {
  int queue_cap = 3;
  SingleThreadPriorityAsyncExecutor executor(queue_cap);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());

  for (int i = 0; i < queue_cap; i++) {
    std::function<bool()> cancellation_callback;
    auto far_ahead_timestamp =
        (TimeProvider::GetSteadyTimestampInNanoseconds() +
         std::chrono::hours(24))
            .count();

    EXPECT_SUCCESS(executor.ScheduleFor([&]() { EXPECT_EQ(true, false); },
                                        far_ahead_timestamp,
                                        cancellation_callback));

    // Cancel the task
    EXPECT_EQ(cancellation_callback(), true);
  }
  // This should exit quickly and should not get stuck.
  EXPECT_SUCCESS(executor.Stop());
}

}  // namespace
}  // namespace privacy_sandbox::pbs_common
