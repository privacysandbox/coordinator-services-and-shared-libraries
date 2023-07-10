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

#include "roma/worker/src/worker_pool.h"

#include <gtest/gtest.h>

#include <linux/limits.h>

#include <memory>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "include/libplatform/libplatform.h"
#include "roma/ipc/src/ipc_manager.h"

using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::test::AutoInitRunStop;
using google::scp::roma::ipc::IpcManager;
using std::make_unique;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace google::scp::roma::worker::test {
class WorkerPoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!platform_) {
      int my_pid = getpid();
      string proc_exe_path = string("/proc/") + to_string(my_pid) + "/exe";
      auto my_path = std::make_unique<char[]>(PATH_MAX);
      ssize_t sz = readlink(proc_exe_path.c_str(), my_path.get(), PATH_MAX);
      ASSERT_GT(sz, 0);
      v8::V8::InitializeICUDefaultLocation(my_path.get());
      v8::V8::InitializeExternalStartupData(my_path.get());
      platform_ = v8::platform::NewDefaultPlatform();
      v8::V8::InitializePlatform(platform_.get());
      v8::V8::Initialize();
    }
  }

  static unique_ptr<v8::Platform> platform_;
};

unique_ptr<v8::Platform> WorkerPoolTest::platform_{nullptr};

TEST_F(WorkerPoolTest, InitRunStopTrueV8) {
  unique_ptr<IpcManager> manager(IpcManager::Create(5));
  AutoInitRunStop auto_init_run_stop(*manager);

  WorkerPool worker_pool;
  EXPECT_EQ(worker_pool.Init(), SuccessExecutionResult());
  EXPECT_EQ(worker_pool.Run(), SuccessExecutionResult());
  EXPECT_EQ(worker_pool.Stop(), SuccessExecutionResult());
  IpcManager::Instance()->ReleaseLocks();
  int child_exit_status;
  waitpid(-1, &child_exit_status, 0);
  EXPECT_EQ(WEXITSTATUS(child_exit_status), 0);
}

TEST_F(WorkerPoolTest, InitRunStop) {
  unique_ptr<IpcManager> manager(IpcManager::Create(5));
  AutoInitRunStop auto_init_run_stop(*manager);

  WorkerPool worker_pool;
  EXPECT_EQ(worker_pool.Init(), SuccessExecutionResult());
  EXPECT_EQ(worker_pool.Run(), SuccessExecutionResult());

  // Make sure worker processes are started. Wait until the given worker PID is
  // not -1
  for (int i = 0; i < 5; i++) {
    while (worker_pool.GetWorkerPids().at(i) == -1) {
      usleep(1000);
    }
  }

  EXPECT_EQ(worker_pool.Stop(), SuccessExecutionResult());
  manager->ReleaseLocks();

  int child_exit_status;
  waitpid(worker_pool.GetWorkerStarterPid(), &child_exit_status, 0);
  EXPECT_EQ(WEXITSTATUS(child_exit_status), 0);
}
}  // namespace google::scp::roma::worker::test
