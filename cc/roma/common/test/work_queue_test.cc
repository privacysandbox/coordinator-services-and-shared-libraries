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

#include "roma/common/src/work_queue.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "core/test/utils/conditional_wait.h"
#include "roma/common/src/error_codes.h"
#include "roma/common/src/process.h"
#include "roma/common/src/shm_allocator.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_WORK_QUEUE_POP_FAILURE;
using std::atomic;
using std::function;
using std::make_shared;
using std::shared_ptr;
using std::thread;
using std::vector;
using std::this_thread::yield;

namespace google::scp::roma::common::test {

struct IntWrapper : public ShmAllocated {
  int value;
};

TEST(WorkQueueTest, PushAndPop) {
  char buffer[10240];
  SharedMemoryPool pool(buffer, sizeof(buffer));
  WorkQueue<int> queue(pool);

  for (auto i = 0; i < 100; ++i) {
    queue.Push(i);
  }
  EXPECT_EQ(queue.Size(), 100);

  auto success = core::SuccessExecutionResult();
  int output;
  for (auto i = 0; i < 100; ++i) {
    EXPECT_EQ(queue.Pop(output), success);
    EXPECT_EQ(output, i);
  }

  EXPECT_EQ(queue.Size(), 0);
  EXPECT_EQ(queue.Pop(output),
            FailureExecutionResult(SC_ROMA_WORK_QUEUE_POP_FAILURE));
}

TEST(WorkQueueTest, PushAndPopFunction) {
  char buffer[10240];
  SharedMemoryPool pool(buffer, sizeof(buffer));
  WorkQueue<function<ExecutionResult()>> queue(pool);

  function<ExecutionResult()> callback = []() {
    return FailureExecutionResult(SC_UNKNOWN);
  };

  queue.Push(callback);
  EXPECT_EQ(queue.Size(), 1);
  function<ExecutionResult()> func;
  EXPECT_EQ(queue.Pop(func), core::SuccessExecutionResult());
  EXPECT_EQ(func(), FailureExecutionResult(SC_UNKNOWN));
  EXPECT_EQ(queue.Size(), 0);
}

TEST(WorkQueueTest, MultiProcesses) {
  SharedMemorySegment segment;
  segment.Create(10240);
  auto* pool = new (segment.Get()) SharedMemoryPool();
  pool->Init(reinterpret_cast<uint8_t*>(segment.Get()) + sizeof(*pool),
             segment.Size() - sizeof(*pool));
  pool->SetThisThreadMemPool();

  auto* queue = new WorkQueue<IntWrapper>(*pool);

  ShmAllocator<vector<atomic<uint64_t>>> alloc_bit(*pool);
  ShmAllocator<atomic<uint64_t>> alloc_elm(*pool);
  auto* bit_ptr = alloc_bit.allocate(1);
  auto* bitmap =
      new (bit_ptr) vector<atomic<uint64_t>, ShmAllocator<atomic<uint64_t>>>(
          (1000 + 63) / 64, alloc_elm);

  auto func_1 = [&queue, &bitmap]() {
    for (auto i = 0; i < 100; ++i) {
      int word_idx = i / 64;
      int bit_idx = i % 64;
      uint64_t mask = 1UL << bit_idx;
      auto& word = bitmap->at(word_idx);
      // verify bit is zero and set it.
      EXPECT_EQ(word.fetch_or(mask) & mask, 0);
      IntWrapper wrap;
      wrap.value = i;
      while (!queue->Push(wrap).Successful()) {}
    }
    return SuccessExecutionResult();
  };

  auto func_2 = [&queue, &bitmap]() {
    for (auto i = 0; i < 100; ++i) {
      IntWrapper wrap;
      wrap.value = -1;
      while (!queue->Pop(wrap).Successful()) {}
      int word_idx = wrap.value / 64;
      int bit_idx = wrap.value % 64;
      uint64_t mask = 1UL << bit_idx;
      auto& word = bitmap->at(word_idx);
      // verify bit is set and clear it
      EXPECT_EQ(word.fetch_and(~mask) & mask, mask);
    }

    return SuccessExecutionResult();
  };

  pid_t pid_1, pid_2;
  int child_status_1, child_status_2;

  Process::Create(func_1, pid_1);
  Process::Create(func_2, pid_2);
  waitpid(pid_1, &child_status_1, 0);
  waitpid(pid_2, &child_status_2, 0);
  EXPECT_EQ(WEXITSTATUS(child_status_1), 0);
  EXPECT_EQ(WEXITSTATUS(child_status_2), 0);
  // the queue size should be empty after two processes done.
  EXPECT_EQ(queue->Size(), 0);
}

TEST(WorkQueueTest, MultiThreadedPush) {
  SharedMemorySegment segment;
  segment.Create(10240);
  auto* pool = new (segment.Get()) SharedMemoryPool();
  pool->Init(reinterpret_cast<uint8_t*>(segment.Get()) + sizeof(*pool),
             segment.Size() - sizeof(*pool));
  pool->SetThisThreadMemPool();

  auto* queue = new WorkQueue<IntWrapper>(*pool);

  vector<thread> threads;
  vector<atomic<uint64_t>> bitmap((1000 + 63) / 64);

  for (auto i = 0; i < 100; ++i) {
    threads.push_back(thread([i, &queue, &bitmap]() {
      int index = i;
      int word_idx = index / 64;
      int bit_idx = index % 64;
      uint64_t mask = 1UL << bit_idx;
      // auto& word = bitmap->at(word_idx);
      auto& word = bitmap[word_idx];
      //  verify bit is zero and set it.
      EXPECT_EQ(word.fetch_or(mask) & mask, 0);
      IntWrapper wrap;
      wrap.value = index;
      while (!queue->Push(wrap).Successful()) {}
    }));

    threads.push_back(thread([&queue, &bitmap]() {
      IntWrapper wrap;
      wrap.value = -1;
      while (!queue->Pop(wrap).Successful()) {}
      int word_idx = wrap.value / 64;
      int bit_idx = wrap.value % 64;
      uint64_t mask = 1UL << bit_idx;
      // auto& word = bitmap->at(word_idx);
      auto& word = bitmap[word_idx];
      //  verify bit is set and clear it
      EXPECT_EQ(word.fetch_and(~mask) & mask, mask);
    }));
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // the queue size should be empty after all thread done.
  EXPECT_EQ(queue->Size(), 0);
}

}  // namespace google::scp::roma::common::test
