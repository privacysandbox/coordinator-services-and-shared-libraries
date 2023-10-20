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
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"

namespace google::scp::cpio {
/*! @copydoc SimpleMetricInterface
 */
class SimpleMetric : public SimpleMetricInterface {
 public:
  explicit SimpleMetric(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<MetricClientInterface>& metric_client,
      const std::shared_ptr<MetricDefinition>& metric_info)
      : async_executor_(async_executor),
        metric_client_(metric_client),
        metric_info_(metric_info) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  void Push(const std::shared_ptr<MetricValue>& metric_value,
            const std::shared_ptr<MetricTag>& metric_tag) noexcept override;

 protected:
  /**
   * @brief Runs the actual metric push logic.
   *
   * @param record_metric_request The pointer to an PutMetricsRequest
   * object.
   */
  virtual void RunMetricPush(
      const std::shared_ptr<cmrt::sdk::metric_service::v1::PutMetricsRequest>
          record_metric_request) noexcept;
  /// The cancellation task callback.
  std::function<bool()> current_task_cancellation_callback_;
  /// An instance to the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  /// Metric client instance.
  std::shared_ptr<MetricClientInterface> metric_client_;
  /// Metric general information.
  std::shared_ptr<MetricDefinition> metric_info_;
  /// Acitivity ID of the object
  const core::common::Uuid object_activity_id_;
};

}  // namespace google::scp::cpio
