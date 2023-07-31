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

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>

#include "shared_memory.h"
#include "shm_mutex.h"

namespace google::scp::roma::common {
namespace test {
class LocalPoolFixture;
}

/**
 * @brief A memory pool that's backed by a single fixed size storage segment.
 *
 * This is intended to work in a process-shared environment, so everything
 * should reside inside the storage memory itself. We expect the memory is used
 * to facilitate data structures that are used in queue-like scenarios, meaning
 * that the deallocations are almost in the same order of allocations. With that
 * in mind, we use the following allocation policy:
 *   - If enough memory available at the head or the first free block, allocate
 *     from there;
 *   - Otherwise, allocate from the tail.
 * This way the allocated memory are more cache friendly, and search should
 * succeed in semi-constant time.
 */
class SharedMemoryPool {
  /// The memory blocks, implemented as singly linked list
  struct Block {
    typedef uint32_t size_type;
    Block* next_;
    /// The size of the data. We currently only support 32bit size to save some
    /// memory for small objects.
    size_type data_size_;

    /// Flags. Currently only containing indicators of if this block is free or
    /// allocated.
    std::atomic<uint32_t> flags;
    static constexpr uint32_t kBitAllocated = 0x00000001;

    /// The actual data location. We use max_align_t here to guarantee
    /// alignment. The actual size isn't 1, but defined by \a data_size_. Also
    /// the array size here serves as placeholder to guard minimum block size to
    /// reduce overhead.
    max_align_t data[1];

    explicit Block(size_type data_size)
        : next_(nullptr), data_size_(data_size), flags(0u) {}

    /// Create a Block at specified storage location.
    static Block* Create(void* storage, size_type block_data_size) {
      return new (storage) Block(block_data_size);
    }

    /// Get pointer to the beginning of the data region.
    void* Begin() { return reinterpret_cast<void*>(data); }

    /// Get pointer past the end (analogous to stl "end()" iterators). This
    /// pointer should not be dereferenced, but only used as comparison
    /// subjects.
    void* End() {
      uintptr_t end = reinterpret_cast<uintptr_t>(data) + data_size_;
      return reinterpret_cast<void*>(end);
    }

    /// Set the bit to indicate this block is allocated.
    inline void SetAllocated() {
      flags.fetch_or(kBitAllocated, std::memory_order_relaxed);
    }

    /// Clear the bit to indicate this block is free.
    inline void SetFree() {
      flags.fetch_and(~kBitAllocated, std::memory_order_relaxed);
    }

    /// If this block is free.
    inline bool IsFree() const { return !(flags.load() & kBitAllocated); }

    /// Align up an integral number to the max alignment required.
    template <typename T>
    static T AlignUp(T t) {
      static_assert(std::is_integral_v<T>);
      size_t align = alignof(max_align_t);
      return (t + align - 1) & ~(align - 1);
    }

    /// Align down an integral number to the max alignment required.
    template <typename T>
    static T AlignDown(T t) {
      static_assert(std::is_integral_v<T>);
      size_t align = alignof(max_align_t);
      return t & ~(align - 1);
    }

    /// Merge the next block.
    void MergeNext() {
      // By merging we save the storage of one header, so we need to add it to
      // the data size.
      data_size_ += next_->data_size_ + offsetof(Block, data);
      next_ = next_->next_;
    }

    /// Merge all free blocks starting from current block.
    /// @return number of blocks merged.
    size_t MergeAllFree() {
      size_t count = 0UL;
      for (; next_ != nullptr && next_->IsFree(); ++count) {
        MergeNext();
      }
      return count;
    }

    /// Split this block, retain at least \a size bytes data. User should
    /// guarantee data_size_ has enough space to split.
    /// @return The new block
    Block* Split(size_type size) {
      uintptr_t data_addr = reinterpret_cast<uintptr_t>(data);
      uintptr_t next_base = data_addr + size;
      // The base address of any block should be aligned, so that the data are
      // also aligned.
      next_base = AlignUp(next_base);
      // Calculate the new size of this block. Since we have aligned up the base
      // address of next block, we should have new_size >= size here.
      size_type new_size = next_base - data_addr;
      void* new_block_addr = reinterpret_cast<void*>(next_base);
      size_type new_block_data_size =
          data_size_ - new_size - offsetof(Block, data);
      // construct a new block at the location.
      Block* new_block = Create(new_block_addr, new_block_data_size);
      // Insert into the link list.
      new_block->next_ = next_;
      next_ = new_block;
      data_size_ = new_size;
      return new_block;
    }

    Block* NextFree() {
      auto* block = next_;
      for (; block != nullptr && !block->IsFree(); block = block->next_) {}
      return block;
    }
  };  // struct Block

 public:
  /// A SharedMemoryPool switcher with RAII semantics. It should only be used as
  /// stack objects.
  struct Context {
    explicit Context(SharedMemoryPool& pool) : old(default_mempool_of_thread_) {
      SharedMemoryPool::SetThisThreadMemPool(pool);
    }

    ~Context() { default_mempool_of_thread_ = old; }

    SharedMemoryPool* old;
    // Disable heap allocations, so that it can only be used on stack.
    static void* operator new(size_t) = delete;
    static void* operator new[](size_t) = delete;
    static void operator delete(void*) = delete;
    static void operator delete[](void*) = delete;
  };

  /// Constructs empty memory pool
  SharedMemoryPool();
  /// Constructs the memory pool with allocated \a memory and \a size.
  SharedMemoryPool(void* memory, size_t size);
  /// Initialize the pool with allocated \a memory and \a size.
  void Init(void* memory, size_t size);
  /// Allocate (at least) \a size bytes. See doc of SharedMemoryPool for
  /// allocation strategies.
  [[nodiscard]] void* Allocate(size_t size);
  /// Deallocate memory pointed by \a pointer.
  void Deallocate(void* pointer);

  inline bool operator==(const SharedMemoryPool& other) {
    return base_addr_ == other.base_addr_ && capacity_ == other.capacity_;
  }

  /// Set the default mempool for this thread. This is useful when we have a
  /// thread working on a designated pool, and this allows default-construction
  /// of ShmAllocators, so that it eases the construction of objects.
  static void SetThisThreadMemPool(SharedMemoryPool& pool) {
    default_mempool_of_thread_ = &pool;
  }

  /// Set the default mempool for this thread as this object.
  void SetThisThreadMemPool() { default_mempool_of_thread_ = this; }

  /// Set the default mempool for this thread. This is useful when we have a
  /// thread working on a designated pool, and this allows default-construction
  /// of ShmAllocators, so that it eases the construction of objects.
  static SharedMemoryPool& GetThisThreadMemPool() {
    return *default_mempool_of_thread_;
  }

  /// A helper function to switch MemPools.
  ///     auto ctx = SharedMemoryPool::SwitchTo(pool);
  /// is equivalent to
  ///     SharedMemoryPool::Context ctx(pool);
  [[nodiscard]] static Context SwitchTo(SharedMemoryPool& pool) {
    return Context(pool);
  }

 private:
  friend class test::LocalPoolFixture;
  /// The offset of data relative to the beginning of a Block.
  static constexpr size_t kBlockDataOffset = offsetof(Block, data);

  /// This is the default mem pool of the current thread. Set by
  /// SetThisThreadMemPool().
  static thread_local SharedMemoryPool* default_mempool_of_thread_;

  /// The space remaining at the tail block.
  size_t TailSpace() const;

  /// Try performing allocation from the first free block.
  /// \return the block containing the allocation, or nullptr if failed.
  Block* AllocateFromFirstFree(size_t size);
  /// Try performing allocation from the tail block.
  /// \return the block containing the allocation, or nullptr if failed.
  Block* AllocateFromTail(size_t size);
  /// Try performing allocation by linear searching.
  /// \return the block containing the allocation, or nullptr if failed.
  Block* AllocateByLinearSearch(size_t size);

  /// The base address of the memory segment.
  void* base_addr_;
  /// The full capacity of the memory segment.
  size_t capacity_;
  /// The mutex for allocating from the pool.
  ShmMutex alloc_mutex_;
  /// Number of blocks created in this pool.
  size_t block_count_;
  /// Sum of all allocated sizes, including overheads & paddings.
  std::atomic<size_t> allocated_size_;
  /// The very first block. Address of this block should not change.
  Block* head_;
  /// The first free block on or after head_. This isn't guaranteed to be the
  /// first free block, but a best effort hint.
  std::atomic<Block*> first_free_;
  /// The tail block.
  Block* tail_;
};

}  // namespace google::scp::roma::common
