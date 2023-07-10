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
#include <utility>

#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "shm_allocator.h"
#include "shm_mutex.h"

namespace google::scp::roma::common {
/**
 * @brief WorkQueue provides a queue which allocates the elements to
 * the shared memory pool to support the processes share memory and data with
 * each other.
 */
template <typename T>
class WorkQueue : public ShmAllocated {
  /// The node struct used to implement the linked list of WorkQueue.
  struct Node : public ShmAllocated {
    /// The pointer to the data of the node.
    std::unique_ptr<T> data;
    /// The pointer to the next node.
    std::atomic<Node*> next;

    /**
     * @brief Construct a new Node object.
     *
     * @param data the shared pointer of the data.
     */
    explicit Node(std::unique_ptr<T> data = nullptr)
        : data(std::move(data)), next(nullptr) {}
  };

 public:
  /**
   * @brief Construct a new Work Queue object with shared memory pool.
   *
   * @param shm_pool the shared memory pool used to allocate the elements of the
   * queue.
   */
  explicit WorkQueue(SharedMemoryPool& shm_pool)
      : mem_pool_(shm_pool), size_(0) {
    auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
    Node* const new_node = new Node();
    head_ = new_node;
    tail_ = new_node;
  }

  /**
   * @brief Construct a new Work Queue object with default shared memory pool.
   */
  WorkQueue() : WorkQueue(SharedMemoryPool::GetThisThreadMemPool()) {}

  /**
   * @brief Push an element into the queue. This function is thread-safe.
   *
   * @param element the element to be queued.
   * @return core::ExecutionResult result of the operation.
   */
  core::ExecutionResult Push(std::unique_ptr<T> element) {
    auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
    Node* const new_node = new Node(std::move(element));
    {
      std::lock_guard lock(mutex_tail_);
      tail_->next = new_node;
      tail_ = new_node;

      size_++;
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Push(const T& element) {
    auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
    auto element_ptr = std::make_unique<T>(element);
    return Push(std::move(element_ptr));
  }

  /**
   * @brief Pop an element from the queue. If there is no element the result
   * will contain the proper error code.
   *
   * @param element the element to be dequeued
   * @return core::ExecutionResult result of the operation.
   */
  core::ExecutionResult Pop(std::unique_ptr<T>& element) {
    auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
    std::lock_guard lock(mutex_head_);

    Node* old_head = head_;
    Node* new_head = old_head->next;
    if (!new_head) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_WORK_QUEUE_POP_FAILURE);
    } else {
      element.swap(new_head->data);
      head_ = new_head;
    }

    delete old_head;
    size_--;
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Pop(T& element) {
    auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
    std::unique_ptr<T> element_ptr;
    auto result = Pop(element_ptr);
    if (result == core::SuccessExecutionResult()) {
      element = *element_ptr;
    }
    return result;
  }

  /**
   * @brief Provides the size of the elements in the queue. Due to the nature of
   * the concurrent queue, this value will be approximate.
   * @return size_t number of elements in the queue.
   */
  size_t Size() { return size_.load(); }

 private:
  /// The mutex for the head of the queue.
  ShmMutex mutex_head_;
  /// The mutex for the tail of the queue.
  ShmMutex mutex_tail_;
  /// The memory pool this queue uses. It is used for internal allocations.
  SharedMemoryPool& mem_pool_;
  /// The pointer to the head of the queue.
  Node* head_;
  /// The pointer to the tail of the queue.
  Node* tail_;
  /// Indicates how many elements in the queue.
  std::atomic<size_t> size_;
};
}  // namespace google::scp::roma::common
