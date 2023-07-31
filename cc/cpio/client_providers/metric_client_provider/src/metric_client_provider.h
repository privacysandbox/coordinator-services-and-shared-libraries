
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
#include <mutex>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc MetricClientProviderInterface
 */
class MetricClientProvider : public MetricClientProviderInterface {
 public:
  virtual ~MetricClientProvider() = default;

  explicit MetricClientProvider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<MetricClientOptions>& metric_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider)
      : async_executor_(async_executor),
        metric_client_options_(metric_client_options),
        instance_client_provider_(instance_client_provider),
        is_running_(false),
        active_push_count_(0),
        number_metrics_in_vector_(0) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult PutMetrics(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>&
          record_metric_context) noexcept override;

 protected:
  /**
   * @brief Triggered when PutMetricsRequest arrives.
   *
   * @param context async execution context.
   */
  virtual void OnPutMetrics(
      core::AsyncContext<google::protobuf::Any, google::protobuf::Any>
          context) noexcept;

  /**
   * @brief The actual function to push the metrics received to cloud.
   *
   * @param metric_requests_vector The vector stored record_metric_request
   * contexts.
   */
  virtual core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept = 0;

  /**
   * @brief Schedules a round of metric push in the next time_duration_. The
   * main goal of this feature is to ensure that when the client does not
   * receive enough metrics to trigger a batch push, the metrics will be pushed
   * to the cloud in set time duration.
   *
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult ScheduleMetricsBatchPush() noexcept;

  /**
   * @brief The helper function for ScheduleMetricsBatchPush to do the actual
   * metrics batching and pushing.
   *
   */
  virtual void RunMetricsBatchPush() noexcept;

  /// An instance to the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// The configuration for metric client.
  std::shared_ptr<MetricClientOptions> metric_client_options_;

  /// Instance client provider to fetch cloud metadata.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;

  /// The vector stores the metric record requests received.
  std::vector<
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>>
      metric_requests_vector_;

  /// Indicates whther the component stopped
  bool is_running_;
  /// Number of active metric push.
  std::atomic<size_t> active_push_count_;
  /// Number of metrics received in metric_requests_vector_.
  std::atomic<uint64_t> number_metrics_in_vector_;

  /// The cancellation callback.
  std::function<bool()> current_cancellation_callback_;
  /// Sync mutex
  std::mutex sync_mutex_;
};
}  // namespace google::scp::cpio::client_providers
