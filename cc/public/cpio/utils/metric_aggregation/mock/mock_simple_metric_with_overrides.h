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
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"
#include "public/cpio/utils/metric_aggregation/src/simple_metric.h"

namespace google::scp::cpio {
class MockSimpleMetricOverrides : public SimpleMetric {
 public:
  explicit MockSimpleMetricOverrides(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<MetricClientInterface>& metric_client,
      const std::shared_ptr<MetricDefinition>& metric_info)
      : SimpleMetric(async_executor, metric_client, metric_info) {}

  std::function<void(const std::shared_ptr<MetricValue>&,
                     const std::shared_ptr<MetricTag>&)>
      push_mock;

  std::function<void(
      const std::shared_ptr<cmrt::sdk::metric_service::v1::PutMetricsRequest>)>
      run_metric_push_mock;

  void Push(const std::shared_ptr<MetricValue>& metric_value,
            const std::shared_ptr<MetricTag>& metric_tag =
                nullptr) noexcept override {
    if (push_mock) {
      push_mock(metric_value, metric_tag);
      return;
    }
    return SimpleMetric::Push(metric_value, metric_tag);
  }

  void RunMetricPush(
      const std::shared_ptr<cmrt::sdk::metric_service::v1::PutMetricsRequest>
          record_metric_request) noexcept {
    if (run_metric_push_mock) {
      run_metric_push_mock(record_metric_request);
      return;
    }
    return SimpleMetric::RunMetricPush(record_metric_request);
  }
};

}  // namespace google::scp::cpio
