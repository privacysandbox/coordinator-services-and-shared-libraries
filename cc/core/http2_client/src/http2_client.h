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

#include <memory>

#include "cc/core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "cc/core/http2_client/src/http_connection_pool.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/metrics/meter.h"

namespace privacy_sandbox::pbs_common {

struct HttpClientOptions {
  HttpClientOptions()
      : retry_strategy_options(
            privacy_sandbox::pbs_common::RetryStrategyOptions(
                privacy_sandbox::pbs_common::RetryStrategyType::Exponential,
                kDefaultRetryStrategyDelayInMs,
                kDefaultRetryStrategyMaxRetries)),
        max_connections_per_host(kDefaultMaxConnectionsPerHost),
        http2_read_timeout_in_sec(kDefaultHttp2ReadTimeoutInSeconds) {}

  HttpClientOptions(
      privacy_sandbox::pbs_common::RetryStrategyOptions retry_strategy_options,
      size_t max_connections_per_host, TimeDuration http2_read_timeout_in_sec)
      : retry_strategy_options(retry_strategy_options),
        max_connections_per_host(max_connections_per_host),
        http2_read_timeout_in_sec(http2_read_timeout_in_sec) {}

  /// Retry strategy options.
  const privacy_sandbox::pbs_common::RetryStrategyOptions
      retry_strategy_options;
  /// Max http connections per host.
  const size_t max_connections_per_host;
  /// nghttp client read timeout.
  const TimeDuration http2_read_timeout_in_sec;
};

/*! @copydoc HttpClientInterface
 */
class HttpClient : public HttpClientInterface {
 public:
  /**
   * @brief Constructs a new HttpClient object for making HTTP requests with
   *
   * @param async_executor A shared pointer to an instance of an asynchronous
   * executor responsible for managing background tasks for HTTP request
   * execution.
   * @param options An optional HttpClientOptions object containing
   * configurations such as timeout, retry strategy, and other HTTP-related
   * settings. Defaults to a default-constructed HttpClientOptions if not
   * provided.
   * @param metric_router An optional pointer to a MetricRouter object used for
   * creating HTTP client metrics. Defaults to nullptr if not provided.
   */
  explicit HttpClient(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      HttpClientOptions options = HttpClientOptions(),
      absl::Nullable<google::scp::core::MetricRouter*> metric_router = nullptr);

  google::scp::core::ExecutionResult Init() noexcept override;
  google::scp::core::ExecutionResult Run() noexcept override;
  google::scp::core::ExecutionResult Stop() noexcept override;

  google::scp::core::ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept override;

 private:
  /**
   * Increments the client connection creation error counter.
   */
  void IncrementClientConnectionCreationError(
      const AsyncContext<HttpRequest, HttpResponse>& http_context);

  // An instance of the connection pool that is used by the http client.
  std::unique_ptr<HttpConnectionPool> http_connection_pool_;

  // Operation dispatcher
  privacy_sandbox::pbs_common::OperationDispatcher operation_dispatcher_;

  // An instance of metric router which will provide APIs to create metrics.
  google::scp::core::MetricRouter* metric_router_;

  // OpenTelemetry Meter used for creating and managing metrics.
  std::shared_ptr<opentelemetry::metrics::Meter> meter_;

  // OpenTelemetry Instrument for client connection creation errors.
  std::shared_ptr<opentelemetry::metrics::Counter<uint64_t>>
      client_connection_creation_error_counter_;
};
}  // namespace privacy_sandbox::pbs_common
