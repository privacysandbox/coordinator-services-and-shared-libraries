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

#include "roma/common/src/process.h"

#include <gtest/gtest.h>

#include <stdlib.h>
#include <sys/wait.h>

#include <atomic>
#include <functional>
#include <future>
#include <iostream>

#include "core/test/utils/conditional_wait.h"
#include "roma/common/src/shared_memory_pool.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::function;

namespace google::scp::roma::common::test {

TEST(ProcessTest, CreateProcessSuccessChildProcess) {
  pid_t pid;
  SharedMemorySegment segment;
  segment.Create(1024);
  atomic<int>* i = new (segment.Get()) atomic<int>(0);

  auto func = [i]() {
    i->store(1);
    return SuccessExecutionResult();
  };
  EXPECT_EQ(Process::Create(func, pid), SuccessExecutionResult());
  EXPECT_GT(pid, 0);

  int child_exit_status;
  waitpid(pid, &child_exit_status, 0);
  EXPECT_EQ(WEXITSTATUS(child_exit_status), 0);
  EXPECT_EQ(i->load(), 1);

  segment.Unmap();
}

TEST(ProcessTest, CreateProcessFailedChildProcess) {
  pid_t pid;
  SharedMemorySegment segment;
  segment.Create(1024);
  atomic<int>* i = new (segment.Get()) atomic<int>(0);
  auto func = [i]() {
    i->store(1);
    return FailureExecutionResult(SC_UNKNOWN);
  };

  EXPECT_EQ(Process::Create(func, pid), SuccessExecutionResult());
  EXPECT_GT(pid, 0);

  int child_exit_status;
  waitpid(pid, &child_exit_status, 0);
  EXPECT_EQ(WEXITSTATUS(child_exit_status), 1);
  EXPECT_EQ(i->load(), 1);
  segment.Unmap();
}

}  // namespace google::scp::roma::common::test
