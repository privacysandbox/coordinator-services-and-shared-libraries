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

#include "core/interface/async_executor_interface.h"
#include "core/interface/partition_types.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"

namespace google::scp::pbs {
/**
 * @brief Metrics wrapper for partition to push metrics.
 *
 * This implements three metrics.
 * 1. Simple Metric for Load duration in Milliseconds.
 * 2. Simple Metric for Load duration in Milliseconds.
 * 3. Aggregate Metric for counting lease related events.
 */
class PartitionMetricsWrapper {
 public:
  PartitionMetricsWrapper(
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const core::PartitionId& partition_id,
      size_t metric_aggregation_interval_milliseconds)
      : metric_client_(metric_client),
        async_executor_(async_executor),
        partition_id_(partition_id),
        metric_aggregation_interval_milliseconds_(
            metric_aggregation_interval_milliseconds) {}

  core::ExecutionResult Init();

  core::ExecutionResult Run();

  core::ExecutionResult Stop();

  /**
   * @brief Emits a metric for Partition Load
   *
   * @param load_duration_in_ms
   */
  void OnPartitionLoaded(size_t load_duration_in_ms);

  /**
   * @brief Emits a metric for Partition Unload
   *
   * @param unload_duration_in_ms
   */
  void OnPartitionUnloaded(size_t unload_duration_in_ms);

  /**
   * @brief Emits a metric for Partition Lease Renewed
   *
   */
  void OnLeaseRenewed();

 protected:
  std::shared_ptr<cpio::SimpleMetricInterface> load_duration_metric_;
  std::shared_ptr<cpio::SimpleMetricInterface> unload_duration_metric_;
  std::shared_ptr<cpio::AggregateMetricInterface> lease_event_metrics_;

  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  const core::PartitionId partition_id_;
  const size_t metric_aggregation_interval_milliseconds_;
};
}  // namespace google::scp::pbs
