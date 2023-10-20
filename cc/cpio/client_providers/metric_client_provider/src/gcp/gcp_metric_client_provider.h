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
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "cpio/client_providers/metric_client_provider/src/metric_client_provider.h"
#include "google/cloud/future.h"
#include "google/cloud/monitoring/metric_client.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc MetricClientProvider
 */
class GcpMetricClientProvider : public MetricClientProvider {
 public:
  explicit GcpMetricClientProvider(
      const std::shared_ptr<MetricClientOptions>& metric_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor =
          nullptr,
      const std::shared_ptr<MetricBatchingOptions>& metric_batching_options =
          std::make_shared<MetricBatchingOptions>())
      : MetricClientProvider(async_executor, metric_client_options,
                             instance_client_provider,
                             metric_batching_options) {}

  GcpMetricClientProvider() = delete;

  core::ExecutionResult Run() noexcept override;

 protected:
  explicit GcpMetricClientProvider(
      std::shared_ptr<google::cloud::monitoring::MetricServiceClient>
          metric_service_client,
      const std::shared_ptr<MetricClientOptions>& metric_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor =
          nullptr,
      const std::shared_ptr<MetricBatchingOptions>& metric_batching_options =
          std::make_shared<MetricBatchingOptions>())
      : MetricClientProvider(async_executor, metric_client_options,
                             instance_client_provider, metric_batching_options),
        metric_service_client_(std::move(metric_service_client)) {}

  virtual void CreateMetricServiceClient() noexcept;

  core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept override;

  /**
   * @brief Is called after GCP AsyncCreateTimeSeries is completed.
   *
   * @param record_metric_context the record custom metric operation
   * context.
   * @param outcome the operation outcome of GCP AsyncCreateTimeSeries.
   */
  virtual void OnAsyncCreateTimeSeriesCallback(
      std::vector<
          core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                             cmrt::sdk::metric_service::v1::PutMetricsResponse>>
          metric_requests_vector,
      google::cloud::future<google::cloud::Status> outcome) noexcept;

 private:
  GcpInstanceResourceNameDetails instance_resource_;

  /// An Instance of the Gcp metric service client.
  std::shared_ptr<const google::cloud::monitoring::MetricServiceClient>
      metric_service_client_;
};

}  // namespace google::scp::cpio::client_providers
