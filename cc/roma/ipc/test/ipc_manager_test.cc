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

#include "roma/ipc/src/ipc_manager.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "core/test/utils/conditional_wait.h"

using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::unique_ptr;
using std::vector;
using ::testing::KilledBySignal;

namespace google::scp::roma::ipc::test {
using common::RoleId;

TEST(IpcManagerTest, ExplicitShare) {
  // using a unique_ptr so that we deallocate after test done
  unique_ptr<IpcManager> manager(IpcManager::Create(5));
  AutoInitRunStop auto_init_run_stop(*manager);
  // Create an atomic int on each segment.
  vector<atomic<int>*> values;
  for (int i = 0; i < 5; ++i) {
    auto ipc_ctx = IpcManager::Instance()->SwitchTo(RoleId(i));
    auto& ipc = manager->GetIpcChannel();
    auto alloc = ipc.GetAllocator<atomic<int>>();
    auto* int_ptr = alloc.allocate(1);
    int_ptr->store(0);
    values.push_back(int_ptr);
    EXPECT_TRUE(ipc_ctx.old_role.Bad());
  }
  for (int i = 0; i < 5; ++i) {
    pid_t child_pid = fork();
    if (child_pid == 0) {
      IpcManager::Instance()->SetUpIpcForMyProcess(RoleId(i, false));
      atomic<int>* p = values[i];
      p->store(0xBEEF);
      exit(0);
    }
  }
  for (int i = 0; i < 5; ++i) {
    WaitUntil([&, i]() { return values[i]->load() == 0xBEEF; });
    EXPECT_EQ(values[i]->load(), 0xBEEF);
  }
}

TEST(IpcManagerTest, AccessFault) {
  // using a unique_ptr so that we deallocate after test done
  unique_ptr<IpcManager> manager(IpcManager::Create(5));
  AutoInitRunStop auto_init_run_stop(*manager);
  atomic<int>* int_ptr = nullptr;
  {
    auto ipc_ctx = IpcManager::Instance()->SwitchTo(RoleId(0));
    auto& ipc = manager->GetIpcChannel();
    auto alloc = ipc.GetAllocator<atomic<int>>();
    int_ptr = alloc.allocate(1);
  }
  // Assume we are worker 2
  manager->SetUpIpcForMyProcess(RoleId(2, false));
  // accessing int_ptr should cause segfault
  EXPECT_EXIT((*int_ptr = 10), KilledBySignal(SIGSEGV), ".*");
}

}  // namespace google::scp::roma::ipc::test
