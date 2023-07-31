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

#include "roma/common/src/shm_semaphore.h"

#include <gtest/gtest.h>

#include <functional>
#include <thread>
#include <vector>

#include "roma/common/src/process.h"
#include "roma/common/src/shm_allocator.h"

using google::scp::core::SuccessExecutionResult;
using std::thread;
using std::vector;

namespace google::scp::roma::common::test {

TEST(ShmSemaphoreTest, MultiProcessWaitAndSignal) {
  /**
   * @brief Need to create a shared memory segment as the semaphore must live in
   * in shared memory so that different processes can acces it.
   */
  SharedMemorySegment segment;
  segment.Create(10240);
  auto* pool = new (segment.Get()) SharedMemoryPool();
  pool->Init(reinterpret_cast<uint8_t*>(segment.Get()) + sizeof(*pool),
             segment.Size() - sizeof(*pool));
  void* sem_memory = pool->Allocate(sizeof(ShmSemaphore));
  /// Allocate the semaphore in shared memory
  ShmSemaphore* sem = new (sem_memory) ShmSemaphore(0);

  auto wait_process = [&pool, &sem]() {
    /// Switch the context of this process to use shared memory
    auto ctx = SharedMemoryPool::SwitchTo(*pool);

    auto result = sem->WaitOne();
    EXPECT_EQ(result, SuccessExecutionResult());

    return SuccessExecutionResult();
  };

  auto signal_process = [&pool, &sem]() {
    /// Switch the context of this process to use shared memory
    auto ctx = SharedMemoryPool::SwitchTo(*pool);

    auto result = sem->Signal();
    EXPECT_EQ(result, SuccessExecutionResult());

    return SuccessExecutionResult();
  };

  pid_t pid1 = -1;
  pid_t pid2 = -1;
  int child_status1, child_status2;

  auto result1 = Process::Create(wait_process, pid1);
  auto result2 = Process::Create(signal_process, pid2);

  EXPECT_EQ(result1, SuccessExecutionResult());
  EXPECT_EQ(result2, SuccessExecutionResult());
  waitpid(pid1, &child_status1, 0);
  waitpid(pid2, &child_status2, 0);
  EXPECT_EQ(WEXITSTATUS(child_status1), 0);
  EXPECT_EQ(WEXITSTATUS(child_status2), 0);
}

TEST(ShmSemaphoreTest, MultiThreadWaitAndSignal) {
  /// No need for shared memory as these threads belong to one process
  ShmSemaphore sem(0);
  vector<thread> threads;

  for (auto i = 0; i < 100; ++i) {
    threads.push_back(thread([&sem]() {
      auto result = sem.WaitOne();
      EXPECT_EQ(result, SuccessExecutionResult());
    }));

    threads.push_back(thread([&sem]() {
      auto result = sem.Signal();
      EXPECT_EQ(result, SuccessExecutionResult());
    }));
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

TEST(ShmSemaphoreTest, TryWaitShouldFailWhenSemaphoreIsTaken) {
  ShmSemaphore sem(0);
  EXPECT_FALSE(sem.TryWait().Successful());

  sem.Signal();

  // It was signaled so we should be able to take it
  EXPECT_TRUE(sem.TryWait().Successful());
  // It was taken so we should not be able to take it
  EXPECT_FALSE(sem.TryWait().Successful());
}
}  // namespace google::scp::roma::common::test
