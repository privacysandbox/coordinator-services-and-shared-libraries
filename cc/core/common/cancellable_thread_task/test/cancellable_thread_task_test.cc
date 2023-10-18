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

#include "core/common/cancellable_thread_task/src/cancellable_thread_task.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::test::WaitUntil;
using std::atomic;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::this_thread::sleep_for;

namespace google::scp::core::common {
TEST(CancellableThreadTaskTest, CreateTaskAndWaitUntilComplete) {
  atomic<bool> is_done(false);
  CancellableThreadTask task([&is_done]() { is_done = true; });
  WaitUntil([&is_done]() { return is_done == true; });
}

TEST(CancellableThreadTaskTest,
     CreateTaskAndWaitUntilCompleteWithStartupDelay) {
  atomic<bool> is_done(false);
  size_t startup_delay_in_seconds = 2;
  auto start_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();
  auto end_timestamp = start_timestamp;  // init end to start as a default
  CancellableThreadTask task(
      [&is_done, &end_timestamp]() {
        end_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();
        is_done = true;
      },
      seconds(startup_delay_in_seconds));
  WaitUntil([&is_done]() { return is_done == true; });
  EXPECT_GE(duration_cast<seconds>(end_timestamp - start_timestamp).count(),
            seconds(startup_delay_in_seconds).count());
}

TEST(CancellableThreadTaskTest, CreateTaskAndCancel) {
  atomic<bool> is_done(false);
  size_t startup_delay_in_seconds = 10;
  CancellableThreadTask task([&is_done]() { is_done = true; },
                             seconds(startup_delay_in_seconds));

  // Task can be cancelled.
  EXPECT_TRUE(task.Cancel());
}

TEST(CancellableThreadTaskTest, CannotCancelWhileExecuting) {
  atomic<bool> should_exit(false);
  CancellableThreadTask task([&should_exit]() {
    while (!should_exit) {
      std::this_thread::yield();
    }
  });
  // Wait for a bit for the task to start executing..
  sleep_for(seconds(1));

  // Task cannot be cancelled
  EXPECT_FALSE(task.Cancel());
  should_exit = true;

  // Wait until the task is completed.
  while (!task.IsCompleted()) {
    std::this_thread::yield();
  }
}
}  // namespace google::scp::core::common
