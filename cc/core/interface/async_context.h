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

#include <chrono>
#include <functional>
#include <memory>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"

#include "errors.h"
#include "type_def.h"

namespace google::scp::core {
/**
 * @brief AsyncContext is used to control the lifecycle of any async operations.
 * The caller with set the request, response, and the callback on the object and
 * components will use it to transition from one async state another.
 *
 * @tparam TRequest request template param
 * @tparam TResponse response template param
 */
template <typename TRequest, typename TResponse>
struct AsyncContext {
  /// Type of callback function for the async operations using.
  using Callback =
      typename std::function<void(AsyncContext<TRequest, TResponse>&)>;

  AsyncContext()
      : AsyncContext(
            nullptr /* request */, [](AsyncContext<TRequest, TResponse>&) {},
            common::kZeroUuid, common::kZeroUuid) {}

  /**
   * @brief Constructs a new Async Context object.
   * @param request instance of the request.
   * @param callback the callback object for when the async operation is
   * completed.
   */
  AsyncContext(const std::shared_ptr<TRequest>& request,
               const Callback& callback)
      : AsyncContext(request, callback, common::kZeroUuid, common::kZeroUuid) {}

  /**
   * @brief Constructs a new Async Context object.
   * @param request instance of the request.
   * @param callback the callback object for when the async operation is
   * completed.
   * @param parent_activity_id The parent activity id of the current async
   * context.
   */
  AsyncContext(const std::shared_ptr<TRequest>& request,
               const Callback& callback, const common::Uuid& parent_activity_id)
      : AsyncContext(request, callback, parent_activity_id, common::kZeroUuid) {
  }

  /**
   * @brief Constructs a new Async Context object.
   * @param request instance of the request.
   * @param callback the callback object for when the async operation is
   * completed.
   * @param parent_context The parent async context of the current async
   * context.
   */
  template <typename ParentAsyncContext>
  AsyncContext(const std::shared_ptr<TRequest>& request,
               const Callback& callback,
               const ParentAsyncContext& parent_context)
      : AsyncContext(request, callback, parent_context.activity_id,
                     parent_context.correlation_id) {}

  /**
   * @brief Constructs a new Async Context object.
   * @param request instance of the request.
   * @param callback the callback object for when the async operation is
   * completed.
   * @param parent_activity_id The parent activity id of the current async
   * context.
   * @param correlation_id The correlation id of the current async context.
   */
  AsyncContext(const std::shared_ptr<TRequest>& request,
               const Callback& callback, const common::Uuid& parent_activity_id,
               const common::Uuid& correlation_id)
      : parent_activity_id(parent_activity_id),
        activity_id(common::Uuid::GenerateUuid()),
        correlation_id(correlation_id),
        request(request),
        response(nullptr),
        result(FailureExecutionResult(SC_UNKNOWN)),
        callback(callback),
        retry_count(0) {
    expiration_time =
        (common::TimeProvider::GetSteadyTimestampInNanoseconds() +
         std::chrono::seconds(kAsyncContextExpirationDurationInSeconds))
            .count();
  }

  AsyncContext(const AsyncContext& right) {
    parent_activity_id = right.parent_activity_id;
    activity_id = right.activity_id;
    correlation_id = right.correlation_id;
    request = right.request;
    response = right.response;
    result = right.result;
    callback = right.callback;
    retry_count = right.retry_count;
    expiration_time = right.expiration_time;
  }

  /// Finishes the async operation by calling the callback.
  virtual void Finish() noexcept {
    if (callback) {
      if (!result.Successful()) {
        // typeid(TRequest).name() is an approximation of the context's template
        // types mangled in compiler defined format, mainly for debugging
        // purposes.
        SCP_ERROR_CONTEXT("AsyncContext", (*this), result,
                          "AsyncContext Finished. Mangled RequestType: '%s', "
                          "Mangled ResponseType: '%s'",
                          typeid(TRequest).name(), typeid(TResponse).name());
      }
      callback(*this);
    }
  }

  /// The parent id of the current context.
  common::Uuid parent_activity_id;

  /// The id of the current context.
  common::Uuid activity_id;

  /// The unique id for the operation the current context is relate to.
  /// For example, in CMRTIO, it could be for a request, and in PBS, it could be
  /// for a transaction.
  common::Uuid correlation_id;

  /// The input request for the operation.
  std::shared_ptr<TRequest> request;

  /// The output response for the operation.
  std::shared_ptr<TResponse> response;

  /// The execution result of the operation.
  ExecutionResult result;

  /// Callback function after the execution is done.
  Callback callback;

  /// The count of retries on the request.
  size_t retry_count;

  /// The expiration_time time of the async context.
  Timestamp expiration_time;
};

/**
 * @brief Finish Context on a thread on the provided AsyncExecutor thread pool.
 * Assigns the result to the context, schedules Finish(), and
 * returns the result. If the context cannot be finished async, it will be
 * finished synchronously on the current thread.
 * @param result execution result of operation.
 * @param context the async context to be completed.
 * @param async_executor the executor (thread pool) for the async context to
 * be completed on.
 * @param priority the priority for the executor. Defaults to High.
 */
template <typename TRequest, typename TResponse>
void FinishContext(
    const ExecutionResult& result, AsyncContext<TRequest, TResponse>& context,
    const std::shared_ptr<AsyncExecutorInterface>& async_executor,
    AsyncPriority priority = AsyncPriority::High) {
  context.result = result;

  // Make a copy of context - this way we know async_executor's handle will
  // never go out of scope.
  if (!async_executor
           ->Schedule([context]() mutable { context.Finish(); }, priority)
           .Successful()) {
    context.Finish();
  }
}

/**
 * @brief Finish Context on the current thread.
 * Assigns the result to the context, schedules Finish(), and returns the
 * result.
 * @param result execution result of operation.
 * @param context the async context to be completed.
 */
template <typename TRequest, typename TResponse>
void FinishContext(const ExecutionResult& result,
                   AsyncContext<TRequest, TResponse>& context) {
  context.result = result;
  context.Finish();
}

}  // namespace google::scp::core
