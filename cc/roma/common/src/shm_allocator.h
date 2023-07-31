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

#include "shared_memory_pool.h"

namespace google::scp::roma::common {

/**
 * @brief An allocator that satisfies the requirements of "Allocator", which
 * allocates in the designated shared memory pool.
 */
template <typename T>
class ShmAllocator {
 public:
  using value_type = T;
  using size_type = size_t;

  /// Constructs an object with specified mem pool.
  explicit ShmAllocator(SharedMemoryPool& mem_pool) : mem_pool_(mem_pool) {}

  /// Constructs a default constructor using the default mem pool of current
  /// thread.
  ShmAllocator() : mem_pool_(SharedMemoryPool::GetThisThreadMemPool()) {}

  /// Construct the allocator by rebinding from ShmAllocator of a different
  /// type \a U.
  template <typename U>
  ShmAllocator(const ShmAllocator<U>& other) : mem_pool_(other.mem_pool_) {}

  /// Allocate \a n bytes.
  T* allocate(size_type n) {
    auto p = mem_pool_.Allocate(n * sizeof(T));
    if (p == nullptr) {
      throw std::bad_alloc();
    }
    return static_cast<T*>(p);
  }

  /// Deallocate \a p.
  void deallocate(T* p, size_type) noexcept { mem_pool_.Deallocate(p); }

 protected:
  // These friend classes are needed for comparing and rebinding.
  template <typename A, typename B>
  friend bool operator==(const ShmAllocator<A>& a, const ShmAllocator<B>& b);
  template <typename U>
  friend class ShmAllocator;

  /// The mem pool used by this allocator.
  SharedMemoryPool& mem_pool_;
};

template <typename T, typename U>
inline bool operator==(const ShmAllocator<T>& a, const ShmAllocator<U>& b) {
  return a.mem_pool_ == b.mem_pool_;
}

template <typename T, typename U>
inline bool operator!=(const ShmAllocator<T>& a, const ShmAllocator<U>& b) {
  return !(a == b);
}

struct ShmAllocated {
  static void* operator new(size_t count) {
    return SharedMemoryPool::GetThisThreadMemPool().Allocate(count);
  }

  static void* operator new[](size_t count) {
    return SharedMemoryPool::GetThisThreadMemPool().Allocate(count);
  }

  static void operator delete(void* ptr) {
    return SharedMemoryPool::GetThisThreadMemPool().Deallocate(ptr);
  }

  static void operator delete[](void* ptr) {
    return SharedMemoryPool::GetThisThreadMemPool().Deallocate(ptr);
  }
};

/// A deleter that does nothing. This is useful when we need unique_ptr but the
/// memory needs to be managed manually.
struct NoOpDelete {
  void operator()(void*) {}
};

}  // namespace google::scp::roma::common
