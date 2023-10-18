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

#include <gtest/gtest.h>

#include <functional>
#include <vector>

#include "core/async_executor/src/single_thread_async_executor.h"
#include "core/common/time_provider/src/time_provider.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::TimeProvider;
using std::atomic;
using std::cout;
using std::endl;
using std::function;
using std::make_shared;
using std::rand;
using std::shared_ptr;
using std::thread;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::core::test {
class SingleThreadAsyncExecutorBenchmarkTest : public ::testing::Test {
 protected:
  void SetUpExecutor() {
    size_t queue_size = 100000000;
    bool drop_tasks_on_stop = false;
    async_executor_ =
        make_shared<SingleThreadAsyncExecutor>(queue_size, drop_tasks_on_stop);
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());
  }

  int num_threads_scheduling_tasks_ = 10;
  int task_schedule_count_per_thread_ = 1000000;
  shared_ptr<SingleThreadAsyncExecutor> async_executor_;
  atomic<int64_t> execution_count_ = 0;
  function<void()> test_work_function_ = [&]() {
    execution_count_ += 1;
    execution_count_ += 1;
    execution_count_ += 1;
    execution_count_ += 1;
    execution_count_ += 1;
  };
};

TEST_F(SingleThreadAsyncExecutorBenchmarkTest, PerfTestSmallTask) {
  GTEST_SKIP();
  SetUpExecutor();
  atomic<bool> start = false;
  auto task_queueing_function = [&](int id) {
    while (!start) {}
    for (int i = 0; i < task_schedule_count_per_thread_; i++) {
      EXPECT_SUCCESS(
          async_executor_->Schedule(test_work_function_, AsyncPriority::High));
    }
  };

  vector<thread> threads;
  for (int i = 0; i < num_threads_scheduling_tasks_; i++) {
    threads.emplace_back(task_queueing_function, i);
  }

  // Start workload
  auto start_ns = TimeProvider::GetSteadyTimestampInNanoseconds();
  start = true;
  while (execution_count_ != (num_threads_scheduling_tasks_ *
                              task_schedule_count_per_thread_ * 5)) {
    sleep_for(milliseconds(5));
  }
  auto end_ns = TimeProvider::GetSteadyTimestampInNanoseconds();

  cout << (duration_cast<milliseconds>(end_ns - start_ns)).count()
       << " milliseconds elapsed" << endl;

  EXPECT_SUCCESS(async_executor_->Stop());
  EXPECT_EQ(execution_count_.load(), 5 * num_threads_scheduling_tasks_ *
                                         task_schedule_count_per_thread_);
  for (int i = 0; i < num_threads_scheduling_tasks_; i++) {
    threads[i].join();
  }
}

TEST_F(SingleThreadAsyncExecutorBenchmarkTest, PerfTestSmallTaskMixedPriority) {
  GTEST_SKIP();
  SetUpExecutor();
  atomic<bool> start = false;
  auto task_queueing_function = [&](int id) {
    while (!start) {}
    for (int i = 0; i < task_schedule_count_per_thread_; i++) {
      auto seed = static_cast<uint32_t>(
          TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks());
      auto priority = ((rand_r(&seed) % 2) == 0) ? AsyncPriority::High
                                                 : AsyncPriority::Normal;
      EXPECT_SUCCESS(async_executor_->Schedule(test_work_function_, priority));
    }
  };

  vector<thread> threads;
  for (int i = 0; i < num_threads_scheduling_tasks_; i++) {
    threads.emplace_back(task_queueing_function, i);
  }

  // Start workload
  auto start_ns = TimeProvider::GetSteadyTimestampInNanoseconds();
  start = true;
  while (execution_count_ != (num_threads_scheduling_tasks_ *
                              task_schedule_count_per_thread_ * 5)) {
    sleep_for(milliseconds(5));
  }
  auto end_ns = TimeProvider::GetSteadyTimestampInNanoseconds();

  cout << (duration_cast<milliseconds>(end_ns - start_ns)).count()
       << " milliseconds elapsed" << endl;

  EXPECT_SUCCESS(async_executor_->Stop());
  EXPECT_EQ(execution_count_.load(), 5 * num_threads_scheduling_tasks_ *
                                         task_schedule_count_per_thread_);
  for (int i = 0; i < num_threads_scheduling_tasks_; i++) {
    threads[i].join();
  }
}
}  // namespace google::scp::core::test
