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

#include "core/common/concurrent_queue/src/concurrent_queue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "core/test/scp_test_base.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::common::ConcurrentQueue;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;

using std::atomic;
using std::thread;
using std::vector;
using std::this_thread::yield;

namespace google::scp::core::common::test {

class ConcurrentQueueTests : public ScpTestBase {};

TEST_F(ConcurrentQueueTests, CreateQueueTest) {
  ConcurrentQueue<int> queue(10);

  EXPECT_EQ(queue.Size(), 0);
}

TEST_F(ConcurrentQueueTests, ErrorOnMaxSize) {
  ConcurrentQueue<int> queue(0);

  int i = 1;
  auto result = queue.TryEnqueue(i);

  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          errors::SC_CONCURRENT_QUEUE_CANNOT_ENQUEUE)));
}

TEST_F(ConcurrentQueueTests, ErrorOnNoElement) {
  ConcurrentQueue<int> queue(1);

  int i;
  auto result = queue.TryDequeue(i);

  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          errors::SC_CONCURRENT_QUEUE_CANNOT_DEQUEUE)));
}

TEST_F(ConcurrentQueueTests, MultiThreadedEnqueue) {
  ConcurrentQueue<int> queue(100);

  vector<thread> threads;
  vector<atomic<uint64_t>> bitmap((1000 + 63) / 64);

  for (auto i = 0; i < 1000; ++i) {
    threads.push_back(thread([i, &queue, &bitmap]() {
      int word_idx = i / 64;
      int bit_idx = i % 64;
      uint64_t mask = 1UL << bit_idx;
      auto& word = bitmap[word_idx];
      const auto success = SuccessExecutionResult();
      // verify bit is zero and set it.
      EXPECT_EQ(word.fetch_or(mask) & mask, 0);
      auto index = i;
      while (queue.TryEnqueue(index) != success) {
        yield();
      }
    }));

    threads.push_back(thread([&queue, &bitmap]() {
      int index = -1;
      auto success = SuccessExecutionResult();
      while (queue.TryDequeue(index) != success) {
        yield();
      }
      int word_idx = index / 64;
      int bit_idx = index % 64;
      uint64_t mask = 1UL << bit_idx;
      auto& word = bitmap[word_idx];
      // verify bit is set and clear it
      EXPECT_EQ(word.fetch_and(~mask) & mask, mask);
    }));
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // the queue size should be empty after all thread done.
  EXPECT_EQ(queue.Size(), 0);
}
}  // namespace google::scp::core::common::test
