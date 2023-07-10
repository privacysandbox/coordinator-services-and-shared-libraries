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

#include "roma/common/src/shm_allocator.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "core/test/utils/conditional_wait.h"
#include "roma/common/src/containers.h"
#include "roma/common/src/shared_memory_pool.h"

using google::scp::core::test::WaitUntil;
using std::atomic;
using std::make_unique;
using std::string;
using std::vector;

namespace google::scp::roma::common::test {

template <typename T>
using xvec = vector<T, ShmAllocator<T>>;

TEST(ShmAllocatorTest, requirements) {
  char buffer[1024];
  SharedMemoryPool pool(buffer, sizeof(buffer));
  ShmAllocator<int> alloc(pool);

  ShmAllocator<float> alloc2(alloc);
  ShmAllocator<double> alloc3(std::move(alloc2));
  EXPECT_EQ(alloc2, ShmAllocator<double>(alloc));
}

TEST(ShmAllocatorTest, vector) {
  char buffer[1024];
  SharedMemoryPool pool(buffer, sizeof(buffer));
  ShmAllocator<int> alloc(pool);
  {
    xvec<int> v(alloc);
    v.reserve(100);
    for (int i = 0; i < 100; ++i) {
      v.push_back(i);
    }
    for (int i = 0; i < 100; ++i) {
      EXPECT_EQ(v[i], i);
    }
  }
  {
    auto ptr = std::allocate_shared<xvec<int>>(alloc, alloc);
    ptr->reserve(100);
    for (int i = 0; i < 100; ++i) {
      ptr->push_back(i);
    }
    for (int i = 0; i < 100; ++i) {
      EXPECT_EQ(ptr->at(i), i);
    }
  }
}

struct AllocTest : public ShmAllocated {
  xvec<int> data;
};

TEST(ShmAllocatorTest, DefaultAllocator) {
  SharedMemorySegment memory;
  memory.Create(1024);
  SharedMemoryPool pool(memory.Get(), memory.Size());
  SharedMemoryPool::SetThisThreadMemPool(pool);
  auto p = make_unique<AllocTest>();
  pid_t pid = fork();
  if (pid == 0) {
    WaitUntil([&p]() { return p->data.size() > 0; });
    auto& ref = p->data[0];
    ref = 1;
    exit(0);
  }
  p->data.emplace_back(0);
  auto& ref = p->data[0];
  WaitUntil([&ref]() { return ref == 1; });
  EXPECT_EQ(ref, 1);
}

TEST(ShmAllocatorTest, ContainerConversions) {
  SharedMemorySegment memory;
  memory.Create(1024);
  SharedMemoryPool pool(memory.Get(), memory.Size());
  SharedMemoryPool::SetThisThreadMemPool(pool);

  vector<string> std_vec{"Foo", "Bar", "hello", "world"};
  RomaVector<RomaString> vec(std_vec.begin(), std_vec.end());
  for (auto i = 0u; i < vec.size(); ++i) {
    EXPECT_EQ(string(vec[i]), std_vec[i]);
  }
}

}  // namespace google::scp::roma::common::test
