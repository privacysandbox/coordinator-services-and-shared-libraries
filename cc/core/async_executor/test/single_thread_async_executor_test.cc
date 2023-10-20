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

#include "core/async_executor/src/single_thread_async_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

#include "core/async_executor/mock/mock_async_executor_with_internals.h"
#include "core/async_executor/src/error_codes.h"
#include "core/async_executor/src/typedef.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/test/test_config.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::common::TimeProvider;
using std::atomic;
using std::make_shared;
using std::string;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using testing::Values;

namespace google::scp::core::test {
TEST(SingleThreadAsyncExecutorTests, CannotInitWithTooBigQueueCap) {
  SingleThreadAsyncExecutor executor(kMaxQueueCap + 1);
  EXPECT_THAT(executor.Init(),
              ResultIs(FailureExecutionResult(
                  errors::SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP)));
}

TEST(SingleThreadAsyncExecutorTests, EmptyWorkQueue) {
  SingleThreadAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());
  EXPECT_SUCCESS(executor.Stop());
}

TEST(SingleThreadAsyncExecutorTests, CannotRunTwice) {
  SingleThreadAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());
  EXPECT_THAT(executor.Run(), ResultIs(FailureExecutionResult(
                                  errors::SC_ASYNC_EXECUTOR_ALREADY_RUNNING)));
  EXPECT_SUCCESS(executor.Stop());
}

TEST(SingleThreadAsyncExecutorTests, CannotStopTwice) {
  SingleThreadAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_SUCCESS(executor.Run());
  EXPECT_SUCCESS(executor.Stop());
  EXPECT_THAT(
      executor.Stop(),
      ResultIs(FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadAsyncExecutorTests, CannotScheduleWorkBeforeInit) {
  SingleThreadAsyncExecutor executor(10);
  EXPECT_THAT(
      executor.Schedule([]() {}, AsyncPriority::Normal),
      ResultIs(FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadAsyncExecutorTests, CannotScheduleWorkBeforeRun) {
  SingleThreadAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_THAT(
      executor.Schedule([]() {}, AsyncPriority::Normal),
      ResultIs(FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadAsyncExecutorTests, CannotRunBeforeInit) {
  SingleThreadAsyncExecutor executor(10);
  EXPECT_THAT(executor.Run(), ResultIs(FailureExecutionResult(
                                  errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED)));
}

TEST(SingleThreadAsyncExecutorTests, CannotStopBeforeRun) {
  SingleThreadAsyncExecutor executor(10);
  EXPECT_SUCCESS(executor.Init());
  EXPECT_THAT(
      executor.Stop(),
      ResultIs(FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

TEST(SingleThreadAsyncExecutorTests, ExceedingQueueCapSchedule) {
  int queue_cap = 1;
  SingleThreadAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  {
    // Blocking queue with enough work
    executor.Schedule([&]() { std::this_thread::sleep_for(seconds(5)); },
                      AsyncPriority::Normal);

    // try to push more than the queue can handle
    auto start_time = high_resolution_clock::now();
    while (true) {
      auto result = executor.Schedule([&]() {}, AsyncPriority::Normal);

      if (result ==
          RetryExecutionResult(errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP)) {
        break;
      }

      auto end_time = high_resolution_clock::now();
      auto diff = end_time - start_time;
      if (diff > seconds(5)) {
        FAIL() << "Queue cap schedule was never exceeded.";
      }
    }
  }

  executor.Stop();
}

TEST(SingleThreadAsyncExecutorTests, CountWorkSingleThread) {
  int queue_cap = 10;
  SingleThreadAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();
  {
    atomic<int> count(0);
    for (int i = 0; i < queue_cap / 2; i++) {
      executor.Schedule([&]() { count++; }, AsyncPriority::Normal);
      executor.Schedule([&]() { count++; }, AsyncPriority::High);
    }
    // Waits some time to finish the work.
    WaitUntil([&]() { return count == queue_cap; });
    EXPECT_EQ(count, queue_cap);
  }
  executor.Stop();
}

class AffinityTest : public testing::TestWithParam<size_t> {
 protected:
  size_t GetCpu() const { return GetParam(); }
};

TEST_P(AffinityTest, CountWorkSingleThreadWithAffinity) {
  int queue_cap = 10;
  SingleThreadAsyncExecutor executor(queue_cap, false, GetCpu());
  executor.Init();
  executor.Run();
  {
    atomic<int> count(0);
    for (int i = 0; i < queue_cap / 2; i++) {
      executor.Schedule(
          [&]() {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
            if (GetCpu() < std::thread::hardware_concurrency()) {
              EXPECT_NE(CPU_ISSET(GetCpu(), &cpuset), 0);
            }
            count++;
          },
          AsyncPriority::Normal);
      executor.Schedule([&]() { count++; }, AsyncPriority::High);
    }
    // Waits some time to finish the work.
    WaitUntil([&]() { return count == queue_cap; });
    EXPECT_EQ(count, queue_cap);
  }
  executor.Stop();
}

// The test should work for any value, even an invalid CPU #.
INSTANTIATE_TEST_SUITE_P(SingleThreadAsyncExecutorTests, AffinityTest,
                         Values(0, 1, std::thread::hardware_concurrency() - 1,
                                std::thread::hardware_concurrency()));

TEST(SingleThreadAsyncExecutorTests, CannotScheduleHiPri) {
  int queue_cap = 50;
  SingleThreadAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  EXPECT_THAT(executor.Schedule([&]() {}, AsyncPriority::Urgent),
              ResultIs(FailureExecutionResult(
                  errors::SC_ASYNC_EXECUTOR_INVALID_PRIORITY_TYPE)));
  executor.Stop();
}

TEST(SingleThreadAsyncExecutorTests, CountWorkMultipleThread) {
  int queue_cap = 50;
  SingleThreadAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  atomic<int> count(0);
  for (int i = 0; i < queue_cap / 2; i++) {
    executor.Schedule([&]() { count++; }, AsyncPriority::Normal);
    executor.Schedule([&]() { count++; }, AsyncPriority::High);
  }
  // Waits some time to finish the work.
  WaitUntil([&]() { return count == queue_cap; });
  executor.Stop();

  EXPECT_EQ(count, queue_cap);
}

TEST(SingleThreadAsyncExecutorTests, AsyncContextCallback) {
  SingleThreadAsyncExecutor executor(10);
  executor.Init();
  executor.Run();

  {
    // Atomic is not used here because we just reserve one thread in the
    size_t callback_count = 0;
    auto request = make_shared<string>("request");
    auto callback = [&](AsyncContext<string, string>& context) {
      callback_count++;
    };
    auto context = AsyncContext<string, string>(request, callback);

    executor.Schedule(
        [&]() {
          context.response = make_shared<string>("response");
          context.result = SuccessExecutionResult();
          context.Finish();
        },
        AsyncPriority::Normal);

    // Waits some time to finish the work.
    WaitUntil([&]() { return callback_count == 1; });

    executor.Schedule(
        [&]() {
          context.response = make_shared<string>("response");
          context.result = SuccessExecutionResult();
          context.Finish();
        },
        AsyncPriority::High);
    WaitUntil([&]() { return callback_count == 2; });

    // Verifies the work is executed.
    EXPECT_EQ(*(context.response), "response");
    EXPECT_SUCCESS(context.result);
    // Verifies the callback is executed.
    EXPECT_EQ(callback_count, 2);
  }

  executor.Stop();
}

TEST(SingleThreadAsyncExecutorTests, FinishWorkWhenStopInMiddle) {
  int queue_cap = 6;
  SingleThreadAsyncExecutor executor(queue_cap);
  executor.Init();
  executor.Run();

  atomic<int> normal_count(0);
  atomic<int> medium_count(0);
  for (int i = 0; i < queue_cap / 2; i++) {
    executor.Schedule(
        [&]() {
          normal_count++;
          std::this_thread::sleep_for(UNIT_TEST_SHORT_SLEEP_MS);
        },
        AsyncPriority::Normal);

    executor.Schedule(
        [&]() {
          medium_count++;
          std::this_thread::sleep_for(UNIT_TEST_SHORT_SLEEP_MS);
        },
        AsyncPriority::High);
  }

  executor.Stop();

  // Waits some time to finish the work.
  WaitUntil([&]() { return medium_count + normal_count == queue_cap; });

  EXPECT_EQ(medium_count + normal_count, queue_cap);
}
}  // namespace google::scp::core::test
