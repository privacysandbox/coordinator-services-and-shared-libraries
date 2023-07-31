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

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/timeseries_metric_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AggregateMetricInterface
 */
class TimeSeriesMetric : public TimeSeriesMetricInterface {
 public:
  explicit TimeSeriesMetric(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<MetricClientProviderInterface>& metric_client,
      const std::shared_ptr<MetricDefinition>& metric_info,
      const core::TimeDuration& time_duration);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  void Increment() noexcept override;

  void IncrementBy(uint64_t incrementer) noexcept override;

  void Decrement() noexcept override;

  void DecrementBy(uint64_t decrementer) noexcept override;

  void Push(const std::shared_ptr<TimeEvent>& time_event) noexcept override;

 protected:
  /**
   * @brief Generates a PutMetricsRequest based on the input value and
   * metric_tag, and pushes the metric to cloud.
   *
   * @param value The value of the metric to be pushed.
   * @param metric_tag The metric tag contains the updated metric unit and
   * additional labels that used to identify the specific metric.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult MetricPushHandler(
      const std::shared_ptr<MetricValue>& value,
      const std::shared_ptr<MetricTag>& metric_tag) noexcept;

  /**
   * @brief Runs the actual metric push logic. This operation must be
   * error free to avoid memory increases overtime. In the case of errors an
   * alert must be raised.
   */
  virtual void RunMetricPush() noexcept;

  /**
   * @brief Schedules a round of metric push in the next time_duration_.
   *
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult ScheduleMetricPush() noexcept;

  /// An instance to the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  /// Metric client instance.
  std::shared_ptr<MetricClientProviderInterface> metric_client_;
  /// Metric general information.
  std::shared_ptr<MetricDefinition> metric_info_;

  /// The time duration of the aggregated metric in milliseconds.
  core::TimeDuration time_duration_;

  /// The cancellation callback.
  std::function<bool()> current_cancellation_callback_;
  /// Sync mutex
  std::mutex sync_mutex_;

  /// The metric tag for cumulative time metric.
  std::shared_ptr<MetricTag> cumulative_time_tag_;

  /// The metric tag for counter metric.
  std::shared_ptr<MetricTag> counter_tag_;

  /// Cumulative counter for one event in the time duration.
  uint64_t counter_;

  /// Cumulative time for one event in the time duration.
  core::TimeDuration accumulative_time_;

  /// Indicates whther the component stopped
  bool is_running_;
};

}  // namespace google::scp::cpio::client_providers
