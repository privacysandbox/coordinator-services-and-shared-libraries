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
#include <memory>

#include "public/core/interface/execution_result.h"
#include "roma/common/src/containers.h"
#include "roma/common/src/shm_allocator.h"
#include "roma/common/src/shm_mutex.h"
#include "roma/common/src/shm_semaphore.h"

#include "error_codes.h"
#include "ipc_message.h"

namespace google::scp::roma::ipc {
using common::RomaVector;
using common::SharedMemoryPool;
using common::ShmMutex;
using common::ShmSemaphore;

/**
 * @brief Work container that behaves as a queue.
 * The dispatcher will call Add with a new work item.
 * A worker will call Acquire to get a handle to the request, and then the
 * worker will call Complete to provide a response for the work item.
 * Subsequently, the dispatcher will call GetCompleted to remove work items that
 * are done.
 *
 * The expected use case is that the dispatcher process can call Add from
 * multiple threads. However, GetCompleted is expected to be called from a
 * single thread in the dispatcher process. Also, Acquire and Complete are
 * expected to be called by the same, single thread, in a synchronous manner
 * from the worker process.
 */
class WorkContainer : public ShmAllocated {
 public:
  /**
   * @brief Construct a new Work Container object
   *
   * @param shm_pool A reference to the shared memory pool
   * @param capacity The capacity of the circular buffer used by the container
   */
  explicit WorkContainer(SharedMemoryPool& shm_pool, size_t capacity = 1024)
      : mem_pool_(shm_pool),
        acquire_semaphore_(0),
        complete_semaphore_(0),
        space_available_semaphore_(capacity),
        capacity_(capacity),
        size_(0),
        add_index_(0),
        get_complete_index_(0),
        acquire_index_(0),
        stop_(false) {
    auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
    items_.reserve(capacity);
    for (uint64_t i = 0; i < capacity; i++) {
      items_.push_back(std::make_unique<WorkItem>());
    }
  }

  /**
   * @brief Construct a new work container object with the default shared memory
   * pool.
   */
  WorkContainer() : WorkContainer(SharedMemoryPool::GetThisThreadMemPool()) {}

  /**
   * @brief Try to acquire a slot to add an item into the container. Must be
   * called before calling Add(), and Add() should only be called if this
   * function returns a success.
   *
   * @return core::ExecutionResult
   */
  core::ExecutionResult TryAcquireAdd();

  /**
   * @brief Add a work item to the container. Thead safe. TryAcquireAdd() must
   * be called, and its status checked to be successful before calling Add to
   * make sure there is space in the container.
   *
   * @param work_item
   * @return core::ExecutionResult
   */
  core::ExecutionResult Add(std::unique_ptr<WorkItem> work_item);

  /**
   * @brief Get a request from the container. Not thread safe. Expected to be
   * single-threaded. Note that this function has as an output argument a raw
   * pointer, because it is intended to be used across process boundaries.
   * Meaning that a worker process should not own the pointer, but only be able
   * to work on it. This means that the caller should NOT delete the pointer.
   *
   * @param request
   * @return core::ExecutionResult
   */
  core::ExecutionResult GetRequest(Request*& request);

  /**
   * @brief Complete a work item with the given response. Not thread safe.
   * Expected to be single-threaded.
   *
   * @param response
   * @return core::ExecutionResult
   */
  core::ExecutionResult CompleteRequest(std::unique_ptr<Response> response);

  /**
   * @brief Get a completed work item, effectively removing it from the
   * container. Not thread safe. Expected to be single-threaded.
   *
   * @param work_item
   * @return core::ExecutionResult
   */
  core::ExecutionResult GetCompleted(std::unique_ptr<WorkItem>& work_item);

  /**
   * @brief Get the approximate number of items in the container.
   *
   * @return size_t
   */
  size_t Size();

  /**
   * @brief Calls to functions of the container can be blocking. And we need to
   * make sure that when the service is stopping we allow both the completed
   * work poller (dispatcher) and the workers to exit. So this function releases
   * the sempahores.
   *
   */
  void ReleaseLocks();

  /**
   * @brief Release the lock that is used to get requests from the container.
   */
  void ReleaseGetRequestLock();

 private:
  SharedMemoryPool& mem_pool_;

  ShmSemaphore acquire_semaphore_;
  ShmSemaphore complete_semaphore_;
  ShmSemaphore space_available_semaphore_;

  ShmMutex add_item_mutex_;

  RomaVector<std::unique_ptr<WorkItem>> items_;
  const size_t capacity_;
  std::atomic<size_t> size_;
  uint64_t add_index_;
  uint64_t get_complete_index_;
  uint64_t acquire_index_;

  std::atomic<bool> stop_;
};
}  // namespace google::scp::roma::ipc
