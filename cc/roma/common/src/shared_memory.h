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

#include <stddef.h>
#include <sys/mman.h>

#include "error_codes.h"

namespace google::scp::roma::common {
/**
 * @brief A anonymous shared memory segment that's inheritable by fork. Usage of
 * the shared memory segment should carefully take consideration of the
 * ownership of the segment.
 *
 */
class SharedMemorySegment {
 public:
  SharedMemorySegment() : memory_(nullptr), size_(0UL) {}

  // This data structure should not be copy constructible, so that the ownership
  // is clear.
  SharedMemorySegment(const SharedMemorySegment&) = delete;

  SharedMemorySegment(SharedMemorySegment&& m) noexcept
      : memory_(nullptr), size_(0UL) {
    memory_ = m.memory_;
    size_ = m.size_;
    m.memory_ = nullptr;
    m.size_ = 0UL;
  }

  ~SharedMemorySegment() noexcept { Unmap(); }

  /**
   * @brief Create a shared memory segment.
   *
   * @param size the size of the memory segment. It should be a multiple of the
   * page size.
   */
  core::ExecutionResult Create(size_t size) {
    if (memory_ != nullptr) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_SHARED_MEMORY_INVALID_INIT);
    }
    memory_ = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory_ == MAP_FAILED) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_SHARED_MEMORY_MMAP_FAILURE);
    }
    size_ = size;
    return core::SuccessExecutionResult();
  }

  /// Unmap the shared memory segment from current process's memory space.
  core::ExecutionResult Unmap() noexcept {
    if (memory_ == nullptr) {
      // It's benign to call on an uninitialized shared memory segment.
      return core::SuccessExecutionResult();
    }
    int ret = munmap(memory_, size_);
    if (ret == 0) {
      memory_ = nullptr;
      return core::SuccessExecutionResult();
    }
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_SHARED_MEMORY_UNMAP_FAILURE);
  }

  /// Get the memory
  void* Get() { return memory_; }

  /// Get the size.
  size_t Size() const { return size_; }

 private:
  /// Pointer to the actual memory segment.
  void* memory_;
  /// The size of the shared memory segment.
  size_t size_;
};
}  // namespace google::scp::roma::common
