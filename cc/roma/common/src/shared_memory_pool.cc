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

#include "shared_memory_pool.h"

#include <exception>
#include <limits>
#include <mutex>

using std::lock_guard;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::numeric_limits;

namespace google::scp::roma::common {

thread_local SharedMemoryPool* SharedMemoryPool::default_mempool_of_thread_ =
    nullptr;

SharedMemoryPool::SharedMemoryPool()
    : base_addr_(nullptr),
      capacity_(0UL),
      block_count_(0UL),
      allocated_size_(0UL),
      head_(nullptr),
      first_free_(nullptr),
      tail_(nullptr) {}

SharedMemoryPool::SharedMemoryPool(void* memory, size_t size)
    : base_addr_(memory),
      capacity_(size),
      block_count_(0UL),
      allocated_size_(0UL),
      head_(nullptr),
      first_free_(nullptr),
      tail_(nullptr) {
  Init(memory, size);
}

void SharedMemoryPool::Init(void* memory, size_t size) {
  base_addr_ = memory;
  capacity_ = size;
  head_ = Block::Create(memory, size - kBlockDataOffset);
  first_free_ = head_;
  tail_ = head_;
  block_count_ = 1;
}

size_t SharedMemoryPool::TailSpace() const {
  if (tail_ == nullptr || !tail_->IsFree()) {
    return 0;
  }
  return tail_->data_size_;
}

SharedMemoryPool::Block* SharedMemoryPool::AllocateFromFirstFree(size_t size) {
  Block* try_alloc_from = nullptr;
  do {
    try_alloc_from = first_free_.load(memory_order_relaxed);
    if (try_alloc_from == nullptr) {
      return nullptr;
    }
  } while (!try_alloc_from->IsFree());
  size_t merged_blocks = try_alloc_from->MergeAllFree();
  block_count_ -= merged_blocks;
  // We may have merged all the way to tail. If so, set tail_ correctly.
  tail_ = tail_ < try_alloc_from->End() ? try_alloc_from : tail_;
  // Now check if we have enough space from the block, allocate from it.
  if (try_alloc_from->data_size_ < size) {
    return nullptr;
  }
  try_alloc_from->SetAllocated();
  // If we can split the chosen block after merging all subsequent free
  // blocks, set the new block as first_free_. Otherwise, set first_free_ to
  // null.
  if (try_alloc_from->data_size_ >= size + sizeof(Block)) {
    Block* new_block = try_alloc_from->Split(size);
    block_count_ += 1;
    // If first_free_ isn't changed, set to new_block.
    Block* expected = try_alloc_from;
    first_free_.compare_exchange_strong(
        expected, new_block, memory_order_release, memory_order_relaxed);
    // tail_ may be equal to head_ or first_free_. So we set the correct
    // tail here as well.
    tail_ = tail_ < new_block ? new_block : tail_;
  } else {
    // We cannot make a split of the block, so we occupied the whole block. Now
    // we don't know which block is the first free block, so we just set it to
    // null. Note that we may also linearly search the next free block and set
    // to first_free_, but it may not worth the effort.
    Block* expected = try_alloc_from;
    first_free_.compare_exchange_strong(expected, nullptr, memory_order_release,
                                        memory_order_relaxed);
  }

  allocated_size_ += try_alloc_from->data_size_ + kBlockDataOffset;
  return try_alloc_from;
}

SharedMemoryPool::Block* SharedMemoryPool::AllocateFromTail(size_t size) {
  size_t tail_space = TailSpace();
  if (tail_space < size) {
    // TODO: emit metric on failed allocations.
    return nullptr;
  }
  Block* ret = tail_;
  tail_->SetAllocated();
  if (tail_space >= size + sizeof(Block)) {
    Block* new_block = tail_->Split(size);
    block_count_ += 1;
    allocated_size_ += tail_->data_size_ + kBlockDataOffset;
    tail_ = new_block;
  } else {
    allocated_size_ += tail_->data_size_ + kBlockDataOffset;
  }
  return ret;
}

SharedMemoryPool::Block* SharedMemoryPool::AllocateByLinearSearch(size_t size) {
  if (capacity_ - allocated_size_.load(memory_order_relaxed) <= size) {
    return nullptr;
  }
  // Start from the first free block we can find.
  Block* try_alloc_from = head_->IsFree() ? head_ : head_->NextFree();
  // Go through all the free blocks, merge if possible, and find the first fit
  for (; try_alloc_from != nullptr;
       try_alloc_from = try_alloc_from->NextFree()) {
    auto merged_blocks = try_alloc_from->MergeAllFree();
    // We may have merged all the way to tail. If so, set tail_ correctly.
    tail_ = tail_ < try_alloc_from->End() ? try_alloc_from : tail_;
    block_count_ -= merged_blocks;
    if (try_alloc_from->data_size_ < size) {
      continue;
    }
    // try_alloc_from has enough capacity, allocate from it.
    try_alloc_from->SetAllocated();
    Block* new_block = nullptr;
    // If try_alloc_from is excessively large, split it.
    if (try_alloc_from->data_size_ >= size + sizeof(Block)) {
      new_block = try_alloc_from->Split(size);
      block_count_ += 1;
      tail_ = tail_ < new_block ? new_block : tail_;
    }
    // If this block was the first_free_ or new_block is becoming the first
    // free block, then set first_free_ properly.
    Block* expected_first = first_free_.load();
    while (expected_first == try_alloc_from ||
           (expected_first > new_block && new_block != nullptr)) {
      first_free_.compare_exchange_strong(expected_first, new_block,
                                          memory_order_release,
                                          memory_order_relaxed);
    }
    allocated_size_ += try_alloc_from->data_size_ + kBlockDataOffset;
    return try_alloc_from;
  }
  return nullptr;
}

void* SharedMemoryPool::Allocate(size_t size) {
  lock_guard lock(alloc_mutex_);
  // Try allocating from first free block, if fail then allocate from tail.
  Block* block = AllocateFromFirstFree(size);
  if (block != nullptr) {
    return block->Begin();
  }

  block = AllocateFromTail(size);
  if (block != nullptr) {
    return block->Begin();
  }
  block = AllocateByLinearSearch(size);
  return block == nullptr ? nullptr : block->Begin();
}

void SharedMemoryPool::Deallocate(void* pointer) {
  // Deallocation is mostly just flipping the free bit on the block. Hence we do
  // not need to lock the mutex. This hence requires all changes made to the
  // pool to be consistent and atomic.
  uintptr_t base = reinterpret_cast<uintptr_t>(pointer) - kBlockDataOffset;
  uintptr_t first_block_addr = reinterpret_cast<uintptr_t>(head_);
  uintptr_t last_block_addr = reinterpret_cast<uintptr_t>(tail_);
  if (base < first_block_addr || base > last_block_addr) {
    throw std::runtime_error(
        "Trying to deallocate memory that does not belong to this pool.");
  }
  Block* block = reinterpret_cast<Block*>(base);
  allocated_size_ -= block->data_size_ + kBlockDataOffset;

  // If this block is before first_free_, set as first_free_
  Block* expected = first_free_.load(memory_order_relaxed);
  while ((expected == nullptr || expected > block) &&
         !first_free_.compare_exchange_weak(
             expected, block, memory_order_release, memory_order_relaxed)) {}
  block->SetFree();
}

}  // namespace google::scp::roma::common
