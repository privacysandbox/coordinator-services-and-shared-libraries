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

#ifndef BUFFER_H_
#define BUFFER_H_
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <sys/uio.h>
#endif

#include <deque>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include "freelist.h"

namespace google::scp::proxy {
// A template to convert buffer pointer with length to OS-provided
// scatter-gather buffer type.
template <class SysBufType>
constexpr SysBufType MakeSysBuf(void* buf, size_t len) {
  return SysBufType(buf, len);
}

#ifdef __linux__
using SysBuf = iovec;
#else
struct SysBuf {
  void *data, size_t len;
};
#endif

// Specialization of SysBuf. For other buffer types, e.g. boost::asio::buffer,
// please create a specialization as well for it to work with BasicBuffer<>.
template <>
constexpr SysBuf MakeSysBuf(void* buf, size_t len) {
  return SysBuf{buf, len};
}

// A buffer type for scatter-gather IO, loosely modeled on libevent buffers.
// Internally, the buffer consists of a linked-list of blocks of the same size,
// like a std::deque. Operations are like queue, whereby data is appended to the
// back and consumed from the front.
// To get a set of buffers of total size [sz] to write to, call Reserve(sz).
// When [n] bytes are written, do Commit(n).
// To get available data to consume, call Peek(). When [n] bytes are consumed,
// do Drain(n).
// NOTE: No internal locking is done. Users are expected to add their own
// locking when an object is accessed in multi-threaded environment.
template <size_t BlockSize = 4096>
class BasicBuffer {
 public:
  static constexpr size_t kBlockSize = BlockSize;

  // The basic node of the internal singly linked list.
  // TODO: add memory alignment options.
  struct Block {
    Block* next;    // Pointer to the next block.
    size_t offset;  // Start of the consumable data.
    size_t len;     // Length of the consumable data.
    // The buffer content. We use a GCC extension for zero-length array here
    // https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html
    // At the moment it is supported by major compilers GCC/LLVM/MSVC++.
    char buf[0];

    Block() : next(nullptr), offset(0), len(0) {}

    // Total capacity of each block for storing data.
    static constexpr size_t capacity = kBlockSize - sizeof(Block);

    constexpr char* DataHead() { return &buf[offset]; }

    constexpr size_t DataLen() const { return len; }

    constexpr char* SpaceHead() { return &buf[offset + len]; }

    constexpr size_t SpaceLen() const { return capacity - offset - len; }

    // Commit space of certain length, return remaining length to commit.
    size_t Commit(size_t size) {
      size_t space_len = SpaceLen();
      size_t to_commit = size < space_len ? size : space_len;
      len += to_commit;
      size -= to_commit;
      return size;
    }

    // Drain data of certain length. return remaining length to drain.
    size_t Drain(size_t size) {
      size_t data_len = DataLen();
      size_t to_drain = data_len < size ? data_len : size;
      offset += to_drain;
      len -= to_drain;
      size -= to_drain;
      return size;
    }

    // Allocating a new block from heap.
    static Block* Alloc() {
      void* new_block = nullptr;
      int ret = posix_memalign(&new_block, kBlockSize, kBlockSize);
      if (ret != 0) {
        return nullptr;
      }
      return new (new_block) Block();
    }

    // Deallocating a block from heap.
    static void Dealloc(Block* ptr) { free(ptr); }
  };  // struct Block

  static_assert(BlockSize > sizeof(Block),
                "BlockSize must be bigger than the header of a Block.");

  explicit BasicBuffer(
      const std::shared_ptr<Freelist<Block>>& freelist = nullptr)
      : head_(nullptr),
        end_(nullptr),
        tail_(nullptr),
        freelist_(freelist),
        block_cnt_(0),
        data_size_(0),
        reserved_(false),
        peeked_(false) {
    if (freelist_ == nullptr) {
      freelist_ = std::make_shared<Freelist<Block>>();
    }
  }

  ~BasicBuffer() { freelist_->DeleteChain(head_); }

  // The total remaining space size at the back of the buffer.
  constexpr size_t space_size() const {
    if (head_ == nullptr) {
      return 0;
    }
    // Since data is contiguous, then the space size is basically the total
    // capacity minus the sizeof the data and offset.
    return block_cnt_ * Block::capacity - data_size_ - head_->offset;
  }

  constexpr size_t data_size() const { return data_size_; }

  // Reserve a series of buffer spaces with specified size. SysBufType stands
  // for the OS-provided or framework-provided scatter-gather buffer types,
  // e.g. iovec, boost::asio::buffer, etc.
  template <class SysBufType>
  std::vector<SysBufType> Reserve(size_t size) {
    assert(!reserved_);
    reserved_ = true;
    std::vector<SysBufType> ret;
    SpaceStepper stepper(*this);
    ret.reserve(stepper.NumBuffersToReserve(size));
    size_t remaining = size;
    while (remaining > 0) {
      Block* block = stepper.Next();
      size_t space_len =
          remaining >= block->SpaceLen() ? block->SpaceLen() : remaining;
      ret.push_back(MakeSysBuf<SysBufType>(block->SpaceHead(), space_len));
      remaining -= space_len;
    }
    return ret;
  }

  // Like Reserve(), but in the case that the exact size doesn't matter as long
  // as it meets minimum required. This will effectively reserve space to full
  // blocks to achieve better efficiency. size is updated to the real size of
  // the reserved space.
  template <class SysBufType>
  std::vector<SysBufType> ReserveAtLeast(size_t& size) {
    assert(!reserved_);
    reserved_ = true;
    std::vector<SysBufType> ret;
    SpaceStepper stepper(*this);
    ret.reserve(stepper.NumBuffersToReserve(size));
    size_t reserved_size = 0;
    while (reserved_size < size) {
      Block* block = stepper.Next();
      size_t space_len = block->SpaceLen();
      ret.push_back(MakeSysBuf<SysBufType>(block->SpaceHead(), space_len));
      reserved_size += space_len;
    }
    size = reserved_size;
    return ret;
  }

  // Same as ReserveAtLeast(size&), except that we won't be updating the size.
  // This is useful when the caller passes in an int literal or constant.
  template <class SysBufType>
  std::vector<SysBufType> ReserveAtLeast(const size_t& size) {
    auto reserve_size = size;
    return ReserveAtLeast<SysBufType>(reserve_size);
  }

  // Commit the produced data with specified size.
  void Commit(size_t size) {
    assert(reserved_);
    assert(head_ != nullptr);
    assert(space_size() >= size);
    SpaceStepper stepper(*this);
    size_t remaining = size;
    while (remaining > 0) {
      end_ = stepper.Next();
      remaining = end_->Commit(remaining);
    }
    data_size_ += size;
    reserved_ = false;
  }

  // Get a view of the consumable data inside the buffer.
  template <class SysBufType>
  std::vector<SysBufType> Peek() {
    assert(!peeked_);
    peeked_ = true;
    std::vector<SysBufType> ret;
    ret.reserve(block_cnt_);
    for (Block* b = head_; b != nullptr && b->DataLen() > 0; b = b->next) {
      ret.push_back(MakeSysBuf<SysBufType>(b->DataHead(), b->DataLen()));
    }
    return ret;
  }

  // Mark data of certain size as consumed, that's effectively, to "drain" from
  // the front of the buffer.
  void Drain(size_t size) {
    assert(peeked_);
    assert(size <= data_size_);
    size_t remaining = size;
    while (remaining > 0) {
      remaining = head_->Drain(remaining);
      // If we have exhausted current block, prune it.
      if (head_->DataLen() == 0 && head_->SpaceLen() == 0) {
        Block* next = head_->next;
        freelist_->Delete(head_);
        --block_cnt_;
        head_ = next;
      }
    }
    data_size_ -= size;
    peeked_ = false;
    // We may have pruned the end block when we drain full blocks. If so,
    // set it to head.
    if (data_size_ == 0) {
      end_ = head_;
    }
    // Trivial optimization: if we drained everything and there is no
    // outstanding reserve, then just reset the head block.
    if (data_size_ == 0 && head_ && !reserved_) {
      head_->offset = 0;
    }
  }

  // Copy a single chunk of buffer into this object. The caller should make sure
  // the from buffer is valid and accessible.
  void CopyIn(const void* from, size_t size) {
    auto buffers = Reserve<SysBuf>(size);
    auto* ptr = static_cast<const uint8_t*>(from);
    for (auto& buf : buffers) {
      memcpy(buf.iov_base, ptr, buf.iov_len);
      ptr += buf.iov_len;
    }
    Commit(size);
  }

  // Copy data out to a single chunk of buffer. Returns number of bytes copied.
  // If 'to' is nullptr, the data is discarded.
  size_t CopyOut(void* to, size_t size) {
    auto buffers = Peek<SysBuf>();
    uint8_t* ptr = static_cast<uint8_t*>(to);
    size_t size_copied = 0;
    for (int i = 0; i < buffers.size() && size > 0; ++i) {
      auto& buf = buffers[i];
      size_t remaining = size - size_copied;
      size_t to_copy = remaining > buf.iov_len ? buf.iov_len : remaining;
      if (ptr != nullptr) {
        memcpy(ptr, buf.iov_base, to_copy);
        ptr += to_copy;
      }
      size_copied += to_copy;
    }
    Drain(size_copied);
    return size_copied;
  }

 private:
  // Append a number of blocks, returns the data end block.
  Block* AppendNewBlock(size_t count = 1) {
    if (head_ == nullptr) {
      head_ = freelist_->New();
      tail_ = head_;
      end_ = head_;
      --count;
      ++block_cnt_;
    }
    assert(tail_ != nullptr);
    while (count--) {
      tail_->next = freelist_->New();
      tail_ = tail_->next;
      ++block_cnt_;
    }
    return end_;
  }

  // A convenient wrapper for stepping through the spaces in the block list, for
  // writing purposes, i.e. used by Reserve() and Commit().
  struct SpaceStepper {
    explicit SpaceStepper(BasicBuffer<BlockSize>& buf)
        : buf_(buf), block_(buf_.end_) {
      // In case end_ block as no remaining space, advance the block chain.
      if (block_ != nullptr && block_->SpaceLen() == 0) {
        block_ = block_->next;
      }
    }

    // How many buffers we'll produce when reserving buffer_size bytes.
    size_t NumBuffersToReserve(size_t buffer_size) {
      size_t num_blocks = 0;
      // Any remaining space at the end_ block will be the first buffer.
      if (buf_.end_ != nullptr && buf_.end_->SpaceLen() == 0) {
        num_blocks += 1;
        buffer_size -= buf_.end_->SpaceLen() < buffer_size
                           ? buf_.end_->SpaceLen()
                           : buffer_size;
      }
      num_blocks += (buffer_size + Block::capacity - 1) / Block::capacity;
      return num_blocks;
    }

    // Advance to next step. This essentially advances block_ and return its
    // value prior to advancing.
    Block* Next() {
      // If block_ is null, then we must at the very end. We simply allocate one
      // block and return the new block.
      if (block_ == nullptr) {
        buf_.AppendNewBlock();
        block_ = buf_.tail_;
      }
      Block* ret = block_;
      block_ = block_->next;
      return ret;
    }

    BasicBuffer<BlockSize>& buf_;
    // The current block we are stepping.
    Block* block_;
  };

 private:
  // Start of the block list, this should also be the start block of consumable
  // data, if any, as any empty block in the front will be pruned.
  Block* head_;
  // End block of consumable data.
  Block* end_;
  // Tail of the block list.
  Block* tail_;
  // Freelist for sharing free blocks among different objects and threads.
  std::shared_ptr<Freelist<Block>> freelist_;
  // The number of blocks in the block list.
  size_t block_cnt_;
  // The commited data size.
  size_t data_size_;
  // Indicate if there is an outstanding Reserve().
  bool reserved_;
  // Indicate if there is an outstanding Peek().
  bool peeked_;
};  // BasicBuffer

using Buffer = BasicBuffer<>;
}  // namespace google::scp::proxy

#endif  // BUFFER_H_
