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

#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc MetricClientInterface
 */
class MetricClient : public MetricClientInterface {
 public:
  explicit MetricClient(const std::shared_ptr<MetricClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult PutMetrics(
      core::AsyncContext<
          google::cmrt::sdk::metric_service::v1::PutMetricsRequest,
          google::cmrt::sdk::metric_service::v1::PutMetricsResponse>
          context) noexcept override;

 protected:
  std::shared_ptr<MetricClientInterface> metric_client_provider_;

 private:
  virtual core::ExecutionResult CreateMetricClientProvider() noexcept;

  std::shared_ptr<MetricClientOptions> options_;
};
}  // namespace google::scp::cpio
