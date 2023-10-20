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

#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/streaming_context.h"

#include "error_codes.h"
#include "retry_strategy.h"

namespace google::scp::core::common {

static constexpr char kOperationDispatcher[] = "OperationDispatcher";

/**
 * @brief Provides dispatching mechanism for the callers to automatically retry
 * on the Retry status code.
 */
class OperationDispatcher {
 public:
  /**
   * @brief Construct a new operation dispatcher object.
   *
   * @param async_executor The async executor instance.
   * @param retry_strategy The retry strategy for dispatch operations in case of
   * Retry status code.
   */
  OperationDispatcher(
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      RetryStrategy retry_strategy)
      : async_executor_(async_executor), retry_strategy_(retry_strategy) {}

  /**
   * @brief Dispatches an async_context object to the target component with a
   * provided function.
   *
   * @tparam Context should be AsyncContext<TRequest, TResponse>
   * @param async_context The async context of the operation to be executed.
   * @param dispatch_to_target_function The function to call the target
   * component.
   */
  template <class Context>
  void Dispatch(Context& async_context,
                const std::function<ExecutionResult(Context&)>&
                    dispatch_to_target_function) {
    auto original_callback = async_context.callback;
    async_context.callback = [this, dispatch_to_target_function,
                              original_callback](Context& async_context) {
      if (async_context.result.status == ExecutionStatus::Retry) {
        async_context.retry_count++;
        DispatchWithRetry(async_context, dispatch_to_target_function);
        return;
      }

      original_callback(async_context);
    };

    DispatchWithRetry<Context>(async_context, dispatch_to_target_function);
  }

  /**
   * @brief Dispatches a producer streaming context object to the target
   * component with a provided function.
   *
   * @param producer_streaming_context The streaming context of the operation to
   * be executed.
   * @param dispatch_to_target_function The function to call the target
   * component.
   */
  template <class TRequest, class TResponse>
  void DispatchProducerStreaming(
      ProducerStreamingContext<TRequest, TResponse>& producer_streaming_context,
      const std::function<
          ExecutionResult(ProducerStreamingContext<TRequest, TResponse>&)>&
          dispatch_to_target_function) {
    auto original_callback = producer_streaming_context.callback;
    producer_streaming_context.callback =
        [this, dispatch_to_target_function,
         original_callback](AsyncContext<TRequest, TResponse>& async_context) {
          if (async_context.result.status == ExecutionStatus::Retry) {
            async_context.retry_count++;
            // Downcast is safe here. We must downcast because only one
            // DispatchWithRetry can exist in this compilation unit - the
            // template types must agree.
            DispatchWithRetry(
                static_cast<ProducerStreamingContext<TRequest, TResponse>&>(
                    async_context),
                dispatch_to_target_function);
            return;
          }
          original_callback(async_context);
        };

    DispatchWithRetry(producer_streaming_context, dispatch_to_target_function);
  }

  /**
   * @brief Dispatches a consumer streaming context object to the target
   * component with a provided function.
   *
   * @param consumer_streaming_context The streaming context of the operation to
   * be executed.
   * @param dispatch_to_target_function The function to call the target
   * component.
   */
  template <class TRequest, class TResponse>
  void DispatchConsumerStreaming(
      ConsumerStreamingContext<TRequest, TResponse>& consumer_streaming_context,
      const std::function<
          ExecutionResult(ConsumerStreamingContext<TRequest, TResponse>&)>&
          dispatch_to_target_function) {
    auto original_callback = consumer_streaming_context.process_callback;
    consumer_streaming_context.process_callback =
        [this, dispatch_to_target_function, original_callback](
            ConsumerStreamingContext<TRequest, TResponse>&
                consumer_streaming_context,
            bool is_finish) {
          if (is_finish) {
            if (consumer_streaming_context.result.status ==
                ExecutionStatus::Retry) {
              consumer_streaming_context.retry_count++;
              DispatchWithRetry(consumer_streaming_context,
                                dispatch_to_target_function);
              return;
            }
          }
          original_callback(consumer_streaming_context, is_finish);
        };

    DispatchWithRetry<ConsumerStreamingContext<TRequest, TResponse>>(
        consumer_streaming_context, dispatch_to_target_function);
  }

 private:
  template <class Context>
  void DispatchWithRetry(Context& async_context,
                         const std::function<ExecutionResult(Context&)>&
                             dispatch_to_target_function) {
    auto async_operation = [async_context,
                            dispatch_to_target_function]() mutable {
      auto execution_result = dispatch_to_target_function(async_context);
      if (!execution_result.Successful()) {
        async_context.result = execution_result;
        async_context.Finish();
      }
    };

    // The very first call does not need to be queued.
    if (async_context.retry_count == 0) {
      async_operation();
      return;
    }

    auto back_off_duration_ms =
        retry_strategy_.GetBackOffDurationInMilliseconds(
            async_context.retry_count);

    if (async_context.retry_count >=
        retry_strategy_.GetMaximumAllowedRetryCount()) {
      SCP_ERROR_CONTEXT(kOperationDispatcher, async_context,
                        async_context.result,
                        "Max retries exceeded. Total retries: %lld",
                        async_context.retry_count);
      async_context.result =
          FailureExecutionResult(core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES);
      async_context.Finish();
      return;
    }

    auto current_time =
        TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

    if (async_context.expiration_time <= current_time) {
      SCP_ERROR_CONTEXT(
          kOperationDispatcher, async_context, async_context.result,
          "Async Context expired. Total retries: %lld, Expiration time: %lld",
          async_context.retry_count, async_context.expiration_time);
      async_context.result =
          FailureExecutionResult(core::errors::SC_DISPATCHER_OPERATION_EXPIRED);
      async_context.Finish();
      return;
    }

    auto back_off_duration_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::milliseconds(back_off_duration_ms))
            .count();

    if (async_context.expiration_time - current_time <= back_off_duration_ns) {
      SCP_ERROR_CONTEXT(
          kOperationDispatcher, async_context, async_context.result,
          "Not enough time available for a retry in Async Context. "
          "Total retries: %lld, Expiration time: %lld",
          async_context.retry_count, async_context.expiration_time);
      async_context.result = FailureExecutionResult(
          core::errors::SC_DISPATCHER_NOT_ENOUGH_TIME_REMAINED_FOR_OPERATION);
      async_context.Finish();
      return;
    }

    auto execution_result = async_executor_->ScheduleFor(
        async_operation, current_time + back_off_duration_ns);
    if (!execution_result.Successful()) {
      async_context.result = execution_result;
      async_context.Finish();
    }
  }

  /// An instance of the async executor.
  const std::shared_ptr<AsyncExecutorInterface> async_executor_;
  /// The retry strategy for the dispatcher.
  RetryStrategy retry_strategy_;
};
}  // namespace google::scp::core::common
