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
#include <utility>

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/common/streaming_context/src/error_codes.h"

#include "async_context.h"

namespace google::scp::core {

/**
 * @brief Base class for holding cancellation mechanics for streaming contexts.
 */
template <typename TRequest, typename TResponse>
struct StreamingContext : public AsyncContext<TRequest, TResponse> {
 private:
  using BaseClass = AsyncContext<TRequest, TResponse>;

 public:
  using BaseClass::BaseClass;

  StreamingContext(const StreamingContext& right) : BaseClass(right) {
    is_marked_done = right.is_marked_done;
    is_cancelled = right.is_cancelled;
  }

  /**
   * @brief Marks the streaming context as done. This means that all the
   * messages have been communicated (not necessarily processed) that need to
   * be.
   *
   */
  void MarkDone() noexcept { is_marked_done->store(true); }

  /**
   * @brief Returns true if this context is marked done.
   */
  bool IsMarkedDone() const noexcept { return is_marked_done->load(); }

  /**
   * @brief Attempts to cancel the current operation. This is a best effort
   * cancellation. If cancellation succeeds, it is expected that the callee will
   * still end up calling Finish on this context, although this is not
   * guaranteed. Currently, this is only for users of the SDK/the caller to
   * communicate to the SDK/callee to cancel.
   *
   */
  void TryCancel() noexcept { is_cancelled->store(true); }

  /**
   * @brief Returns true if this context has been tried to be cancelled.
   */
  bool IsCancelled() const noexcept { return is_cancelled->load(); }

 protected:
  std::shared_ptr<std::atomic_bool> is_marked_done =
      std::make_shared<std::atomic_bool>(false);

  std::shared_ptr<std::atomic_bool> is_cancelled =
      std::make_shared<std::atomic_bool>(false);
};

/**
 * @brief ConsumerStreamingContext is used to control the lifecycle of
 * server-side streaming operations. The caller will set the request and the
 * process_callback. process_callback is now called once for each new message
 * placed in the queue and once more when the call has completed. If the
 * context.IsMarkedDone and no element can be acquired, then it is the final
 * call. AsyncContext::callback is now unused.
 *
 * General form of process_callback, using a new TryGetNextResponse to acquire
 * the next response:
 *
 * context.process_callback = [](auto& context, bool is_finish) {
 *   if (is_finish) {
 *     // capture context.result.
 *   }
 *   // It is important that TryGetNextResponse is called before checking
 *   // IsMarkedDone otherwise 2 threads may compete to get the last element and
 *   // enter a bad state.
 *   auto response = context.TryGetNextResponse();
 *   if (response == nullptr) {
 *     if (!context.IsMarkedDone()) {
 *       // Generally, this should be impossible.
 *     }
 *     if (!context.result.Successful()) {
 *       // Handle failure.
 *     } else {
 *       // Handle success.
 *     }
 *   }
 *   // Handle successfully acquiring the next response.
 * };
 *
 * NOTE: There is an edge case where one thread is at #1 and another is at
 * thread #2 (the final element has been dequeued and Finish() has been
 * called.). In this scenario, the thread at #2 *might* assume that all elements
 * in the queue have been dequeued *and* completed processing but the reality
 * may be that processing has not yet completed. If this would result in thread
 * #2 continuing incorrectly, some additional condition should be checked before
 * thread #2 proceeds.
 *
 * @tparam TRequest request template param
 * @tparam TResponse response template param. This is the type of message being
 * acquired with TryGetNextResponse.
 */
template <typename TRequest, typename TResponse>
struct ConsumerStreamingContext : public StreamingContext<TRequest, TResponse> {
 private:
  using BaseClass = StreamingContext<TRequest, TResponse>;

 public:
  /**
   * @brief Construct a new Consumer Streaming Context object.
   *
   * @param max_num_outstanding_responses The maximum number of response objects
   * allowed to not have been acquired by the consumer.
   */
  explicit ConsumerStreamingContext(
      size_t max_num_outstanding_responses = 50000)
      : response_queue(std::make_shared<common::ConcurrentQueue<TResponse>>(
            max_num_outstanding_responses)) {}

  ConsumerStreamingContext(const ConsumerStreamingContext& right)
      : BaseClass(right) {
    response_queue = right.response_queue;
    process_callback = right.process_callback;
  }

  /**
   * @brief Get the next message if available, nullptr otherwise. Messages are
   * returned in the same order as they are placed into the context. NOTE: if
   * nullptr is returned and this context is done, then all messages have been
   * received.
   *
   * Generally, the user of the SDK/caller uses this function.
   */
  std::unique_ptr<TResponse> TryGetNextResponse() noexcept {
    TResponse resp;
    if (!response_queue->TryDequeue(resp).Successful()) {
      return nullptr;
    }
    return std::make_unique<TResponse>(std::move(resp));
  }

  /**
   * @brief Attempts to enqueue resp into the context.
   *
   * Generally, the SDK/callee uses this function.
   *
   * @param resp The message to enqueue.
   * @return ExecutionResult Failure if the context is cancelled or if
   * enqueueing fails. Success otherwise.
   */
  ExecutionResult TryPushResponse(TResponse resp) noexcept {
    if (this->IsMarkedDone()) {
      return FailureExecutionResult(errors::SC_STREAMING_CONTEXT_DONE);
    }
    if (this->IsCancelled()) {
      return FailureExecutionResult(errors::SC_STREAMING_CONTEXT_CANCELLED);
    }
    return response_queue->TryEnqueue(std::move(resp));
  }

  using ProcessCallback = typename std::function<void(
      ConsumerStreamingContext<TRequest, TResponse>&, bool)>;

  // Is called each time a new message is made available AND once more when the
  // async operation is completed. The first argument is *this. The second
  // argument is whether this->result contains the true result of the operation.
  // This is for users to differentiate between ProcessNextMessage calls which
  // do not have meaningful values in this->result, and Finish calls which do.
  ProcessCallback process_callback;

  /// Processes the next message in the queue.
  void ProcessNextMessage() noexcept { this->process_callback(*this, false); }

  /// Finishes the async operation by calling the callback.
  void Finish() noexcept override {
    if (process_callback) {
      if (!this->result.Successful()) {
        // typeid(TRequest).name() is an approximation of the context's template
        // types mangled in compiler defined format, mainly for debugging
        // purposes.
        SCP_ERROR_CONTEXT("AsyncContext", (*this), this->result,
                          "AsyncContext Finished. Mangled RequestType: '%s', "
                          "Mangled ResponseType: '%s'",
                          typeid(TRequest).name(), typeid(TResponse).name());
      }
      process_callback(*this, true);
    }
  }

 private:
  /// ConcurrentQueue used by the callee to communicate messages back to the
  /// caller.
  std::shared_ptr<common::ConcurrentQueue<TResponse>> response_queue;
};

/**
 * @brief ProducerStreamingContext is used to control the lifecycle of
 * client-side streaming operations. The caller will set the request and the
 * callback. AsyncContext's request field should contain the
 * initial request, all subsequent requests (not including the initial) should
 * be communicated via TryPushRequest.
 *
 * @tparam TRequest request template param. This is the type of request being
 * used in TryPushRequest.
 * @tparam TResponse response template param
 */
template <typename TRequest, typename TResponse>
struct ProducerStreamingContext : public StreamingContext<TRequest, TResponse> {
 private:
  using BaseClass = StreamingContext<TRequest, TResponse>;

 public:
  /**
   * @brief Construct a new Producer Streaming Context object
   *
   * @param max_num_outstanding_requests The maximum number of request objects
   * allowed to not have been acquired by the consumer.
   */
  explicit ProducerStreamingContext(size_t max_num_outstanding_requests = 50000)
      : request_queue(std::make_shared<common::ConcurrentQueue<TRequest>>(
            max_num_outstanding_requests)) {}

  ProducerStreamingContext(const ProducerStreamingContext& right)
      : BaseClass(right) {
    request_queue = right.request_queue;
  }

  /**
   * @brief Attempts to enqueue req for processing.
   *
   * Generally, the user of the SDK/caller uses this function.
   *
   * @param req The message to enqueue
   * @return ExecutionResult Whether enqueueing succeeded or not.
   */
  ExecutionResult TryPushRequest(TRequest req) noexcept {
    if (this->IsMarkedDone()) {
      return FailureExecutionResult(errors::SC_STREAMING_CONTEXT_DONE);
    }
    if (this->IsCancelled()) {
      return FailureExecutionResult(errors::SC_STREAMING_CONTEXT_CANCELLED);
    }
    return request_queue->TryEnqueue(std::move(req));
  }

  /**
   * @brief Acquires the next request on the context or nullptr if none is
   * available.
   *
   * Generally, the SDK/callee uses this function.
   */
  std::unique_ptr<TRequest> TryGetNextRequest() noexcept {
    TRequest req;
    if (!request_queue->TryDequeue(req).Successful()) {
      return nullptr;
    }
    return std::make_unique<TRequest>(std::move(req));
  }

 private:
  /// ConcurrentQueue used by the caller to communicate messages to the
  /// callee.
  std::shared_ptr<common::ConcurrentQueue<TRequest>> request_queue;
};

/**
 * @brief Finish Context on a thread on the provided AsyncExecutor thread pool.
 * Assigns the result to the context, schedules Finish(), and
 * returns the result. If the context cannot be finished async, it will be
 * finished synchronously on the current thread. Before finishing the context,
 * we mark the context as done.
 * @param result execution result of operation.
 * @param context the streaming context to be completed.
 * @param async_executor the executor (thread pool) for the async context to
 * be completed on.
 * @param priority the priority for the executor. Defaults to High.
 */
template <template <typename, typename> typename TContext, typename TRequest,
          typename TResponse>
void FinishStreamingContext(
    const ExecutionResult& result, TContext<TRequest, TResponse>& context,
    const std::shared_ptr<AsyncExecutorInterface>& async_executor,
    AsyncPriority priority = AsyncPriority::High) {
  constexpr bool is_consumer_streaming =
      std::is_base_of_v<ConsumerStreamingContext<TRequest, TResponse>,
                        TContext<TRequest, TResponse>>;
  constexpr bool is_producer_streaming =
      std::is_base_of_v<ProducerStreamingContext<TRequest, TResponse>,
                        TContext<TRequest, TResponse>>;
  static_assert(is_consumer_streaming || is_producer_streaming);

  context.result = result;
  context.MarkDone();

  // Make a copy of context - this way we know async_executor's handle will
  // never go out of scope.
  if (!async_executor
           ->Schedule([context]() mutable { context.Finish(); }, priority)
           .Successful()) {
    context.Finish();
  }
}

}  // namespace google::scp::core
