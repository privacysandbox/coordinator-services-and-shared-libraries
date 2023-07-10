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

#ifndef FREELIST_H_
#define FREELIST_H_

#include <atomic>

namespace google::scp::proxy {
// A simple unbounded freelist. Requires T to contain a "next" pointer.
template <typename T>
class Freelist {
 public:
  using BlockType = std::remove_pointer_t<T>;

  Freelist() : head_(nullptr), size_(0) {}

  ~Freelist() { Clear(); }

  // Get a new object from the freelist. If none available, BlockType::Alloc()
  // is called to allocate new block.
  BlockType* New() {
    BlockType* old_head = head_.load();
    if (old_head == nullptr) {
      return BlockType::Alloc();
    }
    while (!head_.compare_exchange_weak(old_head, old_head->next,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {}

    BlockType* ret = old_head;
    size_--;
    // Do initialization before returning.
    return new (ret) BlockType();
  }

  // Dispose a block. The block is given back to the freelist.
  void Delete(BlockType* block) {
    BlockType* new_head = block;
    new_head->next = head_.load();
    while (!head_.compare_exchange_weak(new_head->next, new_head,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {}
    size_++;
  }

  // Dispose a whole block chain. The blocks are given back to the freelist.
  void DeleteChain(BlockType* head) {
    if (head == nullptr) {
      return;
    }
    size_t count = 1;
    BlockType* tail = head;
    for (; tail->next != nullptr; tail = tail->next) {
      ++count;
    }
    tail->next = head_;
    BlockType* new_head = head;
    while (!head_.compare_exchange_weak(tail->next, new_head,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {}
    size_.fetch_add(count);
  }

  // Clear all the blocks saved.
  void Clear() {
    while (head_.load() != nullptr) {
      BlockType* ptr = New();
      BlockType::Dealloc(ptr);
    }
  }

  // Fill the freelist with N blocks.
  void FillN(size_t n) {
    while (n--) {
      BlockType* block = BlockType::Alloc();
      Delete(block);
    }
  }

  size_t Size() const { return size_.load(); }

 private:
  std::atomic<BlockType*> head_;
  std::atomic<size_t> size_;
};  // Freelist
}  // namespace google::scp::proxy

#endif  // FREELIST_H_
