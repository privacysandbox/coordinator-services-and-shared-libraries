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

#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {
class LocalMetricClient : public cpio::MetricClientInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  core::ExecutionResult PutMetrics(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>
          record_metric_context) noexcept override {
    record_metric_context.response =
        std::make_shared<cmrt::sdk::metric_service::v1::PutMetricsResponse>();
    record_metric_context.result = SuccessExecutionResult();
    record_metric_context.Finish();
    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs
