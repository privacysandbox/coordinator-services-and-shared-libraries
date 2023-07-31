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

#include "roma/common/src/shared_memory_pool.h"

#include <gtest/gtest.h>

#include <stdlib.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "core/test/utils/conditional_wait.h"

using google::scp::core::test::WaitUntil;
using std::atomic;
using std::make_unique;
using std::thread;
using std::unique_ptr;
using std::vector;
using std::chrono::high_resolution_clock;

namespace google::scp::roma::common::test {

TEST(SharedMemorySegmentTest, SharingBetweenProcesses) {
  SharedMemorySegment segment;
  segment.Create(1024);
  atomic<int>* i = new (segment.Get()) atomic<int>(0);
  pid_t child = fork();
  if (child == 0) {
    i->store(1);
    exit(0);
  }
  WaitUntil([&]() { return i->load() == 1; });
  EXPECT_EQ(i->load(), 1);
  segment.Unmap();
}

class LocalPoolFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    constexpr size_t mem_size = 1024 * 1024 * 64;
    mem_ = make_unique<char[]>(mem_size);
    pool_ = make_unique<SharedMemoryPool>(mem_.get(), mem_size);
  }

  size_t block_count() const { return pool_->block_count_; }

  size_t allocated_size() const { return pool_->allocated_size_; }

  size_t pool_capacity() const { return pool_->capacity_; }

  constexpr size_t BlockDataOffset() const {
    return SharedMemoryPool::kBlockDataOffset;
  }

  void* HeadData() { return pool_->head_->Begin(); }

  unique_ptr<char[]> mem_;
  unique_ptr<SharedMemoryPool> pool_;
};

TEST_F(LocalPoolFixture, Allocation) {
  vector<void*> allocations;
  allocations.reserve(100);
  for (int i = 0; i < 100; ++i) {
    auto p = pool_->Allocate(1019);  // <- a prime number to test out alignment
    memset(p, 0, 1024);
    allocations.push_back(p);
    uintptr_t ptr_val = reinterpret_cast<uintptr_t>(p);
    EXPECT_EQ(ptr_val & (alignof(max_align_t) - 1), 0);
  }
  EXPECT_EQ(block_count(), 101);
  EXPECT_EQ(allocated_size(), 100 * (1024 + BlockDataOffset()));
  for (auto p : allocations) {
    pool_->Deallocate(p);
  }
  EXPECT_EQ(allocated_size(), 0);

  // After we deallocated everything, we should be able to allocate from head.
  auto p = pool_->Allocate(2393);
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(p);
  EXPECT_EQ(ptr_val & (alignof(max_align_t) - 1), 0);
  EXPECT_EQ(p, HeadData());
}

TEST_F(LocalPoolFixture, MultiThreaded) {
  static constexpr int kNumThreads = 2;
  vector<vector<void*>> allocations(kNumThreads);
  for (auto& v : allocations) {
    v.resize(1000);
  }
  vector<thread> threads;
  threads.reserve(kNumThreads);

  for (int i = 0; i < kNumThreads; ++i) {
    threads.push_back(thread([&, i]() {
      auto& alloc = allocations[i];
      for (int j = 0; j < 1000; ++j) {
        auto p =
            pool_->Allocate(1019);  // <- a prime number to test out alignment
        alloc[j] = p;
        uintptr_t ptr_val = reinterpret_cast<uintptr_t>(p);
        EXPECT_EQ(ptr_val & (alignof(max_align_t) - 1), 0);
      }
    }));
  }
  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(block_count(), kNumThreads * 1000 + 1);
  for (auto& alloc : allocations) {
    for (auto p : alloc) {
      pool_->Deallocate(p);
    }
  }
  EXPECT_EQ(allocated_size(), 0);

  // After we deallocated everything, we should be able to allocate from head.
  auto p = pool_->Allocate(2393);
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(p);
  EXPECT_EQ(ptr_val & (alignof(max_align_t) - 1), 0);
  EXPECT_EQ(p, HeadData());
}

TEST_F(LocalPoolFixture, ContentedMultithreaded) {
  static constexpr int kNumThreads = 10;
  vector<vector<void*>> allocations(kNumThreads);
  for (auto& v : allocations) {
    v.resize(1000);
  }
  vector<thread> threads;
  threads.reserve(kNumThreads);

  for (int i = 0; i < kNumThreads; ++i) {
    threads.push_back(thread([&, i]() {
      auto& alloc = allocations[i];
      for (int j = 0; j < 1000; ++j) {
        auto p =
            pool_->Allocate(1019);  // <- a prime number to test out alignment
        alloc[j] = p;
      }
      for (auto p : alloc) {
        pool_->Deallocate(p);
      }
    }));
  }
  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(allocated_size(), 0);

  // After we deallocated everything, we should be able to allocate from head.
  auto p = pool_->Allocate(2393);
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(p);
  EXPECT_EQ(ptr_val & (alignof(max_align_t) - 1), 0);
  EXPECT_EQ(p, HeadData());
}

TEST_F(LocalPoolFixture, OOM) {
  auto p = pool_->Allocate(pool_capacity() / 2);
  EXPECT_NE(p, nullptr);
  // Since there are overheads, the second alloc should fail.
  auto p_oom = pool_->Allocate(pool_capacity() / 2);
  EXPECT_EQ(p_oom, nullptr);

  // A failed alloc should still leave the pool intact
  auto p_alloc_after_oom = pool_->Allocate(1000);
  EXPECT_NE(p_alloc_after_oom, nullptr);

  // After freeing p, we should be able to allocate.
  pool_->Deallocate(p);
  p = pool_->Allocate(pool_capacity() / 2);
  EXPECT_NE(p, nullptr);
}

// If there's a sticky block after first free block, and when we repeatedly
// allocate & deallocate larger than first free block, then we keep allocating
// at tail and cause fragmentation, and eventually OOM. This is fixed by adding
// AllocateByLinearSearch().
TEST_F(LocalPoolFixture, StickyBlockOOM) {
  auto* first = pool_->Allocate(4);
  auto* sticky = pool_->Allocate(4);
  pool_->Deallocate(first);
  for (size_t i = 0; i < 1024; ++i) {
    auto* mem = pool_->Allocate(pool_capacity() / 1024);
    EXPECT_NE(mem, nullptr);
    pool_->Deallocate(mem);
  }
  pool_->Deallocate(sticky);
}

// Create the following scenario and test the Linear search logic:
// [...][ Allocated ][ ... Free ...][ Allocated][...]
//   ^----- first_free_                           ^
//          tail_ --------------------------------+
TEST_F(LocalPoolFixture, LinearSearchLogic) {
  auto* first = pool_->Allocate(4);
  auto* sticky = pool_->Allocate(4);
  auto* big_chunk = pool_->Allocate(pool_capacity() - 128);
  auto* tail_sticky = pool_->Allocate(4);
  pool_->Deallocate(first);
  pool_->Deallocate(big_chunk);
  *reinterpret_cast<uint32_t*>(sticky) = 0xDEADBEEF;
  *reinterpret_cast<uint32_t*>(tail_sticky) = 0x1337C0DE;

  // This allocation should fail on both first_free and tail block, and end up
  // in the same location of the big_chunk.
  auto* wrapped_around = pool_->Allocate(256);
  EXPECT_EQ(wrapped_around, big_chunk);
  memset(wrapped_around, 0xEF, 256);
  // Verify we haven't corrupted anything
  uint32_t sticky_val = *reinterpret_cast<uint32_t*>(sticky);
  uint32_t tail_sticky_val = *reinterpret_cast<uint32_t*>(tail_sticky);
  EXPECT_EQ(sticky_val, 0xDEADBEEF);
  EXPECT_EQ(tail_sticky_val, 0x1337C0DE);
}

}  // namespace google::scp::roma::common::test
