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
#include <string>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"

namespace google::scp::cpio::client_providers {

/// Configurations for batching metrics.
struct MetricBatchingOptions {
  virtual ~MetricBatchingOptions() = default;

  /**
   * @brief The top level grouping for the application metrics. A
   * typical example would be "/application_name/environment_name".
   * Environment name could be fetched through ConfigClient.
   * Batching only works for the metrics for the same namespace.
   */
  MetricNamespace metric_namespace;
  /**
   * @brief Pushes metrics in batches if true. In most times, when the
   * batch_recording_time_duration is met, the push is triggered. Cloud has
   * its own maximum batch size, and if the maximum batch size is met before the
   * batch_recording_time_duration, the push is triggered too.
   */
  bool enable_batch_recording = false;
  /**
   * @brief The time duration to push metrics when enable_batch_recording is
   * true.
   */
  std::chrono::milliseconds batch_recording_time_duration =
      std::chrono::milliseconds(30000);
};

class MetricClientProviderFactory {
 public:
  /**
   * @brief Factory to create MetricClientProvider.
   *
   * @return std::shared_ptr<MetricClientInterface> created
   * MetricClientProvider.
   */
  static std::shared_ptr<MetricClientInterface> Create(
      const std::shared_ptr<MetricClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);
};
}  // namespace google::scp::cpio::client_providers
