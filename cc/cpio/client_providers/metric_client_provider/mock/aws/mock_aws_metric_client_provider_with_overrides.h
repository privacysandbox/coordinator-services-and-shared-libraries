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
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/metric_client_provider/mock/aws/mock_cloud_watch_client.h"
#include "cpio/client_providers/metric_client_provider/src/aws/aws_metric_client_provider.h"
#include "cpio/client_providers/metric_client_provider/src/metric_client_provider.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/type_def.h"

namespace google::scp::cpio::client_providers::mock {

class MockAwsMetricClientProviderOverrides : public AwsMetricClientProvider {
 public:
  explicit MockAwsMetricClientProviderOverrides(
      const std::shared_ptr<MetricBatchingOptions>& metric_batching_options)
      : AwsMetricClientProvider(
            std::make_shared<MetricClientOptions>(),
            std::make_shared<MockInstanceClientProvider>(),
            std::make_shared<core::async_executor::mock::MockAsyncExecutor>(),
            std::make_shared<core::async_executor::mock::MockAsyncExecutor>(),
            metric_batching_options) {
    std::dynamic_pointer_cast<core::async_executor::mock::MockAsyncExecutor>(
        async_executor_)
        ->schedule_for_mock =
        [&](const core::AsyncOperation& work, Timestamp timestamp,
            std::function<bool()>& cancellation_callback) {
          return core::SuccessExecutionResult();
        };
  }

  std::shared_ptr<MockCloudWatchClient> GetCloudWatchClient() {
    return std::dynamic_pointer_cast<MockCloudWatchClient>(cloud_watch_client_);
  }

  std::shared_ptr<MockInstanceClientProvider> GetInstanceClientProvider() {
    return std::dynamic_pointer_cast<MockInstanceClientProvider>(
        instance_client_provider_);
  }

  std::function<core::ExecutionResult(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&)>
      metric_batch_push_mock;

  core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept {
    if (metric_batch_push_mock) {
      return metric_batch_push_mock(metric_requests_vector);
    }
    return AwsMetricClientProvider::MetricsBatchPush(metric_requests_vector);
  }

  core::ExecutionResult Run() noexcept override {
    auto execution_result = AwsMetricClientProvider::Run();
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    cloud_watch_client_ = std::make_shared<MockCloudWatchClient>();
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::cpio::client_providers::mock
