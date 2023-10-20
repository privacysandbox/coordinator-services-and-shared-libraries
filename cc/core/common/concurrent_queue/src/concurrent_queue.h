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

#include "oneapi/tbb/concurrent_queue.h"

#include "error_codes.h"

namespace google::scp::core::common {
/**
 * @brief ConcurrentQueue provides multi producers and multi consumers queue
 * support to be used generically.
 */
template <class T>
class ConcurrentQueue {
 public:
  /**
   * @brief Construct a new Concurrent Queue object
   * @param max_size Maximum size of the queue
   */
  explicit ConcurrentQueue(size_t max_size)
      : queue_(std::make_unique<tbb::concurrent_bounded_queue<T>>()) {
    queue_->set_capacity(max_size);
  }

  ConcurrentQueue() = delete;

  /**
   * @brief Enqueues an element into the queue if possible. This function is
   * thread-safe.
   * @param element the element to be queued.
   */
  ExecutionResult TryEnqueue(const T& element) noexcept {
    if (!queue_->try_push(element)) {
      return FailureExecutionResult(errors::SC_CONCURRENT_QUEUE_CANNOT_ENQUEUE);
    }
    return SuccessExecutionResult();
  }

  /**
   * @brief Dequeue an element if possible. If there is no element the result
   * will contain the proper error code.
   * @param element the element to be dequeued
   * @return ExecutionResult result of the operation.
   */
  ExecutionResult TryDequeue(T& element) noexcept {
    if (!queue_->try_pop(element)) {
      return FailureExecutionResult(errors::SC_CONCURRENT_QUEUE_CANNOT_DEQUEUE);
    }
    return SuccessExecutionResult();
  }

  /**
   * @brief Provides the size of the elements in the queue. Due to the nature of
   * the concurrent queue, this value will be approximate.
   * @return size_t number of elements in the queue.
   */
  size_t Size() noexcept { return queue_->size(); }

 private:
  /// queue implementation.
  std::unique_ptr<tbb::concurrent_bounded_queue<T>> queue_;
};
}  // namespace google::scp::core::common
