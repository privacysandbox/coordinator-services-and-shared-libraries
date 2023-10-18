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
#include <vector>

#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/metric_client_provider/src/metric_client_provider.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockMetricClientWithOverrides : public MetricClientProvider {
 public:
  explicit MockMetricClientWithOverrides(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<MetricBatchingOptions>& metric_batching_options)
      : MetricClientProvider(async_executor,
                             std::make_shared<MetricClientOptions>(),
                             std::make_shared<MockInstanceClientProvider>(),
                             metric_batching_options) {}

  std::function<core::ExecutionResult(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>&)>
      record_metric_mock;

  std::function<core::ExecutionResult()> schedule_metric_push_mock;
  std::function<core::ExecutionResult(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&)>
      metrics_batch_push_mock;

  std::function<void()> schedule_metrics_helper_mock;

  core::ExecutionResult record_metric_result_mock;

  void RunMetricsBatchPush() noexcept override {
    if (schedule_metrics_helper_mock) {
      return schedule_metrics_helper_mock();
    }
    return MetricClientProvider::RunMetricsBatchPush();
  }

  core::ExecutionResult PutMetrics(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>
          context) noexcept override {
    if (record_metric_mock) {
      return record_metric_mock(context);
    }
    if (record_metric_result_mock) {
      context.result = record_metric_result_mock;
      if (record_metric_result_mock == core::SuccessExecutionResult()) {
        context.response = std::make_shared<
            cmrt::sdk::metric_service::v1::PutMetricsResponse>();
      }
      context.Finish();
      return record_metric_result_mock;
    }

    return MetricClientProvider::PutMetrics(context);
  }

  int GetSizeMetricRequestsVector() {
    return MetricClientProvider::metric_requests_vector_.size();
  }

  core::ExecutionResult ScheduleMetricsBatchPush() noexcept override {
    if (schedule_metric_push_mock) {
      return schedule_metric_push_mock();
    }
    return MetricClientProvider::ScheduleMetricsBatchPush();
  }

  core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept override {
    if (metrics_batch_push_mock) {
      return metrics_batch_push_mock(metric_requests_vector);
    }
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::cpio::client_providers::mock
