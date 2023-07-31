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

#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core::common {
/**
 * @brief SizedOrTimedBytesBuffer provides size vs time buffer functionality.
 * Either when the size of the buffer reaches to certain size or a specifc time
 * has passed, it will force the buffer to flush. The advantage of using this
 * buffer is, data will not be serialized until the very last moment of pushing
 * data to the destination.
 */
template <class TRequest, class TResponse>
class SizedOrTimedBytesBuffer
    : public std::enable_shared_from_this<
          SizedOrTimedBytesBuffer<TRequest, TResponse>> {
 public:
  SizedOrTimedBytesBuffer(
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::function<size_t(AsyncContext<TRequest, TResponse>&)> get_data_size,
      std::function<ExecutionResult(AsyncContext<TRequest, TResponse>&,
                                    BytesBuffer&, size_t&)>
          serialize_function,
      std::function<ExecutionResult(BytesBuffer&,
                                    std::function<void(ExecutionResult&)>)>
          flush_function,
      size_t max_buffer_size, Timestamp expiration_time)
      : initialized_(false),
        sealed_(false),
        reserved_bytes_(0),
        expiration_time_(expiration_time),
        get_data_size_(get_data_size),
        serialize_function_(serialize_function),
        flush_function_(flush_function),
        async_executor_(async_executor) {
    buffer_.capacity = max_buffer_size;
    buffer_.length = 0;
  }

  /**
   * @brief Initializes the buffer object.
   *
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult Init() noexcept {
    auto ptr = this->shared_from_this();
    auto execution_result = async_executor_->ScheduleFor(
        [ptr]() { ptr->Flush(); }, expiration_time_);
    if (execution_result.Successful()) {
      initialized_ = true;
    }
    return execution_result;
  }

  /**
   * @brief Appends data to the current buffer.
   *
   * @param append_context
   * @return ExecutionResult
   */
  ExecutionResult Append(
      AsyncContext<TRequest, TResponse>& append_context) noexcept {
    if (!initialized_) {
      return FailureExecutionResult(
          errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_INITIALIZED);
    }

    buffer_lock_.lock();
    if (sealed_) {
      buffer_lock_.unlock();
      return FailureExecutionResult(
          errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_BUFFER_IS_SEALED);
    }

    size_t bytes_needed = get_data_size_(append_context);
    if (bytes_needed + reserved_bytes_ >= buffer_.capacity) {
      buffer_lock_.unlock();
      auto ptr = this->shared_from_this();
      async_executor_->Schedule([ptr]() { ptr->Flush(); },
                                AsyncPriority::Urgent);
      return FailureExecutionResult(
          errors::SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_ENOUGH_SPACE);
    }

    reserved_bytes_ += bytes_needed;
    appended_data_.push_back(append_context);
    buffer_lock_.unlock();
    return SuccessExecutionResult();
  }

  /**
   * @brief Flushes the current buffer and seals the buffer.
   */
  void Flush() noexcept {
    buffer_lock_.lock();
    if (sealed_) {
      buffer_lock_.unlock();
      // Cannot flush multiple time
      return;
    }

    sealed_ = true;
    buffer_.bytes = std::make_shared<std::vector<Byte>>(buffer_.capacity);
    buffer_lock_.unlock();

    for (auto& append_data : appended_data_) {
      size_t bytes_serialized = 0;
      auto execution_result =
          serialize_function_(append_data, buffer_, bytes_serialized);
      if (!execution_result.Successful()) {
        NotifyAll(execution_result);
        return;
      }
      buffer_.length += bytes_serialized;
    }

    auto ptr = this->shared_from_this();
    auto execution_result =
        flush_function_(buffer_, [ptr](ExecutionResult& execution_result) {
          ptr->NotifyAll(execution_result);
        });

    if (!execution_result.Successful()) {
      NotifyAll(execution_result);
    }
  }

  /**
   * @brief Notifies all the subscribers to the buffer about the outcome of the
   * operation.
   *
   * @param execution_result The execution result of the operation.
   */
  void NotifyAll(ExecutionResult& execution_result) noexcept {
    for (auto& append_data : appended_data_) {
      append_data.result = execution_result;
      append_data.Finish();
    }
  }

 private:
  /// Indicates if the buffer is initialized.
  bool initialized_;
  /// Indicates if the buffer is sealed and cannot accept more data.
  bool sealed_;
  /// Total bytes to be written.
  size_t reserved_bytes_;
  /// Lock to prevent multi thread access to the buffer.
  std::mutex buffer_lock_;
  /// Expiration time of the buffer when will cause the buffer to be sealed.
  Timestamp expiration_time_;
  /// The actual byte buffer holding the data.
  BytesBuffer buffer_;
  /// The function to calculate the size of each append request.
  std::function<size_t(AsyncContext<TRequest, TResponse>&)> get_data_size_;
  /// The function to serialize each request.
  std::function<ExecutionResult(AsyncContext<TRequest, TResponse>&,
                                BytesBuffer&, size_t&)>
      serialize_function_;
  /// The function to flush the data to the persistent storage.
  std::function<ExecutionResult(BytesBuffer&,
                                std::function<void(ExecutionResult&)>)>
      flush_function_;
  /// Vector of all the appended data.
  std::vector<AsyncContext<TRequest, TResponse>> appended_data_;
  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
};
}  // namespace google::scp::core::common
