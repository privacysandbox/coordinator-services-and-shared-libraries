/*
 * Copyright 2025 Google LLC
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

#include "cc/core/http2_client/src/synchronous_http2_client.h"

#include <future>
#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "cc/core/common/operation_dispatcher/src/error_codes.h"
#include "cc/core/common/operation_dispatcher/src/retry_strategy.h"
#include "cc/core/common/time_provider/src/time_provider.h"
#include "cc/core/http2_client/src/http_options.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs_common {

namespace {

class SynchronousExecutor : public AsyncExecutorInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Schedule(const AsyncOperation& work,
                           AsyncPriority) noexcept override {
    work();
    return SuccessExecutionResult();
  }

  ExecutionResult Schedule(const AsyncOperation& work, AsyncPriority priority,
                           AsyncExecutorAffinitySetting) noexcept override {
    work();
    return SuccessExecutionResult();
  }

  ExecutionResult ScheduleFor(const AsyncOperation& work,
                              Timestamp) noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult ScheduleFor(const AsyncOperation& work, Timestamp,
                              AsyncExecutorAffinitySetting) noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult ScheduleFor(const AsyncOperation& work, Timestamp,
                              TaskCancellationLambda&) noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      TaskCancellationLambda& cancellation_callback,
      AsyncExecutorAffinitySetting affinity) noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }
};
}  // namespace

SyncHttpClient::SyncHttpClient(HttpClientOptions options)
    : retry_strategy_(options.retry_strategy_options) {
  std::shared_ptr<AsyncExecutorInterface> sync_executor =
      std::make_shared<SynchronousExecutor>();
  http_connection_pool_ = std::make_unique<HttpConnectionPool>(
      sync_executor, nullptr, options.max_connections_per_host,
      options.http2_read_timeout_in_sec);

  CHECK(http_connection_pool_->Init());
  CHECK(http_connection_pool_->Run());
}

SyncHttpClient::~SyncHttpClient() {
  // Ignore errors if the client failed to stop
  http_connection_pool_->Stop();
}

SyncHttpClientResponse SyncHttpClient::PerformRequest(
    const HttpRequest& http_request) noexcept {
  auto http_request_ptr = std::make_shared<HttpRequest>(http_request);
  std::promise<SyncHttpClientResponse> response_promise;

  AsyncContext<HttpRequest, HttpResponse> http_context(
      http_request_ptr,
      [&response_promise](AsyncContext<HttpRequest, HttpResponse>& context) {
        SyncHttpClientResponse response;
        response.execution_result = context.result;
        if (context.response != nullptr) {
          response.http_response =
              std::make_unique<HttpResponse>(*context.response);
        }
        response_promise.set_value(std::move(response));
      });

  while (http_context.retry_count <
         retry_strategy_.GetMaximumAllowedRetryCount()) {
    auto check_result = CheckForRetries(http_context);
    if (!check_result.Successful()) {
      SyncHttpClientResponse response;
      response.execution_result = check_result.result();
      return response;
    }
    auto expiration_time = *check_result;
    absl::SleepFor(absl::Nanoseconds(expiration_time));

    // The async context callback is only invoked when TryRequest is
    // successfull. So, no race conditions for failures and retries.
    SyncHttpClientResponse response;
    response.execution_result = TryRequest(http_context);

    if (response.execution_result.Successful()) {
      // Wait only when the request was successfully submitted
      response = response_promise.get_future().get();
      // Reset the promise if we have to wait again during retries
      response_promise = std::promise<SyncHttpClientResponse>();
    }

    if (response.execution_result.Retryable()) {
      ++http_context.retry_count;
      continue;
    }
    return response;
  }

  return SyncHttpClientResponse{
      .execution_result =
          FailureExecutionResult(SC_DISPATCHER_EXHAUSTED_RETRIES),
  };
}

ExecutionResult SyncHttpClient::TryRequest(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  std::shared_ptr<HttpConnection> http_connection;
  auto execution_result = http_connection_pool_->GetConnection(
      http_context.request->path, http_connection);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return http_connection->Execute(http_context);
}

ExecutionResultOr<TimeDuration> SyncHttpClient::CheckForRetries(
    AsyncContext<HttpRequest, HttpResponse>& http_context) {
  if (http_context.retry_count == 0) {
    return static_cast<TimeDuration>(0);
  }

  auto back_off_duration_ms = retry_strategy_.GetBackOffDurationInMilliseconds(
      http_context.retry_count);

  auto current_time =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
  if (http_context.expiration_time <= current_time) {
    return FailureExecutionResult(SC_DISPATCHER_OPERATION_EXPIRED);
  }

  auto back_off_duration_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::milliseconds(back_off_duration_ms))
          .count();

  if (http_context.expiration_time - current_time <= back_off_duration_ns) {
    return FailureExecutionResult(
        SC_DISPATCHER_NOT_ENOUGH_TIME_REMAINED_FOR_OPERATION);
  }

  return static_cast<TimeDuration>(back_off_duration_ns);
}

}  // namespace privacy_sandbox::pbs_common
