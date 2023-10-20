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

#include "proxy/src/buffer.h"

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

using std::make_shared;
using std::thread;
using std::unordered_set;
using std::vector;

namespace google::scp::proxy {

// This is exactly like iovec, but to make it more universal, we define it here.
struct TestSysBuf {
  void* data;
  size_t len;
};

template <>
TestSysBuf MakeSysBuf(void* buf, size_t len) {
  return TestSysBuf{buf, len};
}

namespace test {

using TestBuffer = BasicBuffer<64>;
using Block = TestBuffer::Block;
constexpr size_t block_capacity = TestBuffer::Block::capacity;

TEST(FreelistTest, MultiThread) {
  Freelist<Block> freelist;
  vector<Block*> blocks1;
  vector<Block*> blocks2;
  auto alloc = [&freelist](vector<Block*>& blocks) {
    blocks.reserve(100);
    for (int i = 0; i < 100; ++i) {
      blocks.push_back(freelist.New());
    }
  };
  thread t1([&]() { alloc(blocks1); });
  thread t2([&]() { alloc(blocks2); });
  t1.join();
  t2.join();

  EXPECT_EQ(freelist.Size(), 0);
  unordered_set<void*> result_set;
  for (auto block : blocks1) {
    EXPECT_EQ(result_set.count(block), 0);
    result_set.insert(block);
  }
  for (Block* block : blocks2) {
    EXPECT_EQ(result_set.count(block), 0);
    result_set.insert(block);
  }
  EXPECT_EQ(result_set.size(), 200);

  auto dealloc = [&](vector<Block*>& blocks) {
    for (auto block : blocks) {
      freelist.Delete(block);
    }
  };
  thread t3([&]() { dealloc(blocks1); });
  thread t4([&]() { dealloc(blocks2); });
  t3.join();
  t4.join();
  EXPECT_EQ(freelist.Size(), 200);
}

TEST(FreelistTest, MultiThreadDeleteChain) {
  Freelist<Block> freelist;
  Block* head1 = nullptr;
  Block* head2 = nullptr;
  auto alloc = [&freelist](Block** chain_head) {
    *chain_head = nullptr;
    for (int i = 0; i < 100; ++i) {
      Block* new_block = freelist.New();
      new_block->next = *chain_head;
      *chain_head = new_block;
    }
  };
  thread t1([&]() { alloc(&head1); });
  thread t2([&]() { alloc(&head2); });
  t1.join();
  t2.join();

  EXPECT_EQ(freelist.Size(), 0);

  auto dealloc = [&](Block* head) { freelist.DeleteChain(head); };
  thread t3([&]() { dealloc(head1); });
  thread t4([&]() { dealloc(head2); });
  t3.join();
  t4.join();
  EXPECT_EQ(freelist.Size(), 200);
}

// Tests buffer operations Reserve, Commit, Peek, Drain, and common usage
// scenarios. A freelist object is reused among the series of tests to make sure
// the freelist's functionality as well.
TEST(BufferTest, Create) {
  TestBuffer buf;
  EXPECT_EQ(buf.data_size(), 0);
}

TEST(BufferTest, Reserve1) {
  // Reserving size 1 should yield one block.
  TestBuffer buf;
  auto v = buf.Reserve<TestSysBuf>(1);
  EXPECT_EQ(v.size(), 1);
  EXPECT_EQ(v[0].len, 1);

  buf.Commit(1);
  EXPECT_EQ(buf.data_size(), 1);

  auto p = buf.Peek<TestSysBuf>();
  EXPECT_EQ(p.size(), 1);
  EXPECT_EQ(p[0].len, 1);

  buf.Drain(1);
  EXPECT_EQ(buf.data_size(), 0);
}

TEST(BufferTest, ReserveFullBlock) {
  // Reserving the block capacity should yield one block.
  TestBuffer buf;
  auto v = buf.Reserve<TestSysBuf>(block_capacity);
  EXPECT_EQ(v.size(), 1);
  EXPECT_EQ(v[0].len, block_capacity);

  buf.Commit(block_capacity);
  EXPECT_EQ(buf.data_size(), block_capacity);

  auto p = buf.Peek<TestSysBuf>();
  EXPECT_EQ(p.size(), 1);
  EXPECT_EQ(p[0].len, block_capacity);

  buf.Drain(block_capacity);
  EXPECT_EQ(buf.data_size(), 0);
}

TEST(BufferTest, ReserveAtLeast1) {
  // Reserving at least 1 byte should yield a whole block.
  TestBuffer buf;
  auto v = buf.ReserveAtLeast<TestSysBuf>(1);
  EXPECT_EQ(v.size(), 1);
  EXPECT_EQ(v[0].len, block_capacity);
}

TEST(BufferTest, ReserveAtLeast1Twice) {
  // Reserving at least 1 byte should yield a whole block.
  TestBuffer buf;
  auto v = buf.ReserveAtLeast<TestSysBuf>(1);
  EXPECT_EQ(v.size(), 1);
  EXPECT_EQ(v[0].len, block_capacity);
  buf.Commit(block_capacity);

  v = buf.ReserveAtLeast<TestSysBuf>(1);
  EXPECT_EQ(v.size(), 1);
  EXPECT_EQ(v[0].len, block_capacity);
  buf.Commit(block_capacity);

  auto p = buf.Peek<TestSysBuf>();
  EXPECT_EQ(p.size(), 2);
  EXPECT_EQ(p[0].len, block_capacity);
  EXPECT_EQ(p[1].len, block_capacity);
}

TEST(BufferTest, ReserveAtLeastBlockSize1) {
  // Reserving the block capacity + 1 should yield two blocks.
  TestBuffer buf;
  auto v = buf.Reserve<TestSysBuf>(block_capacity + 1);
  EXPECT_EQ(v.size(), 2);
  EXPECT_EQ(v[0].len, block_capacity);
  EXPECT_EQ(v[1].len, 1);

  // Now commit and drain one block.
  buf.Commit(block_capacity);
  EXPECT_EQ(buf.data_size(), block_capacity);
  auto p = buf.Peek<TestSysBuf>();
  buf.Drain(block_capacity);
  EXPECT_EQ(buf.data_size(), 0);
}

TEST(BufferTest, ConsecutiveOps) {
  // Consecutive operations
  TestBuffer buf;
  buf.Reserve<TestSysBuf>(1);
  buf.Commit(1);
  buf.Reserve<TestSysBuf>(5);
  buf.Commit(1);
  buf.Reserve<TestSysBuf>(5);
  buf.Commit(1);
  EXPECT_EQ(buf.data_size(), 3);

  buf.Reserve<TestSysBuf>(block_capacity);
  buf.Commit(block_capacity);
  EXPECT_EQ(buf.data_size(), block_capacity + 3);

  buf.Peek<TestSysBuf>();
  buf.Drain(block_capacity);
  EXPECT_EQ(buf.data_size(), 3);
}

TEST(BufferTest, MultipleBufferObjects) {
  // Multiple buffer objects
  auto freelist = make_shared<Freelist<TestBuffer::Block>>();
  {
    TestBuffer buf1(freelist);
    TestBuffer buf2(freelist);
    buf1.Reserve<TestSysBuf>(1);
    buf1.Commit(1);
    buf2.Reserve<TestSysBuf>(1);
    buf2.Commit(1);
    EXPECT_EQ(buf1.data_size(), 1);
    EXPECT_EQ(buf2.data_size(), 1);
    EXPECT_EQ(freelist->Size(), 0);
  }
  EXPECT_EQ(freelist->Size(), 2);
}

TEST(BufferTest, PrimeSize) {
  TestBuffer buf;
  constexpr size_t reserve_size = 521;  // a prime number
  auto outbuf = buf.Reserve<TestSysBuf>(reserve_size);
  size_t expected_block_cnt =
      (reserve_size + block_capacity - 1) / block_capacity;
  EXPECT_EQ(outbuf.size(), expected_block_cnt);

  size_t space_size = buf.space_size();
  ASSERT_GE(space_size, reserve_size);

  buf.Commit(239);  // another prime
  EXPECT_EQ(buf.data_size(), 239);
  EXPECT_EQ(buf.space_size(), space_size - 239);

  buf.Peek<TestSysBuf>();
  buf.Drain(197);  // another prime
  EXPECT_EQ(buf.data_size(), 239 - 197);
  // space size should remain unchanged.
  EXPECT_EQ(buf.space_size(), space_size - 239);
}

TEST(BufferTest, DrainExactBlock) {
  TestBuffer buf;
  size_t len = 239;
  buf.ReserveAtLeast<TestSysBuf>(len);
  buf.Commit(len);
  buf.ReserveAtLeast<TestSysBuf>(len * 2);
  buf.Commit(len);
  auto buf_vec = buf.Peek<TestSysBuf>();
  EXPECT_GT(buf_vec.size(), 0UL);
  size_t sz = 0;
  for (auto& b : buf_vec) {
    EXPECT_GT(b.len, 0);
    sz += b.len;
  }
  EXPECT_EQ(sz, buf.data_size());
  buf.Drain(sz);

  buf.ReserveAtLeast<TestSysBuf>(len);
  buf.Commit(100);
  buf_vec = buf.Peek<TestSysBuf>();
  EXPECT_GT(buf_vec.size(), 0UL);
  sz = 0;
  for (auto& b : buf_vec) {
    EXPECT_GT(b.len, 0);
    sz += b.len;
  }
  EXPECT_EQ(sz, buf.data_size());
}

TEST(BufferTest, DrainExactBlockToEmptyBuffer) {
  TestBuffer buf;
  size_t len = 239;
  buf.ReserveAtLeast<TestSysBuf>(len);
  buf.Commit(len);
  auto buf_vec = buf.Peek<TestSysBuf>();
  EXPECT_GT(buf_vec.size(), 0UL);
  size_t sz = 0;
  for (auto& b : buf_vec) {
    EXPECT_GT(b.len, 0);
    sz += b.len;
  }
  EXPECT_EQ(sz, buf.data_size());
  buf.Drain(sz);

  buf.ReserveAtLeast<TestSysBuf>(len);
  buf.Commit(100);
  buf_vec = buf.Peek<TestSysBuf>();
  EXPECT_GT(buf_vec.size(), 0UL);
  sz = 0;
  for (auto& b : buf_vec) {
    EXPECT_GT(b.len, 0);
    sz += b.len;
  }
  EXPECT_EQ(sz, buf.data_size());
}

}  // namespace test
}  // namespace google::scp::proxy
