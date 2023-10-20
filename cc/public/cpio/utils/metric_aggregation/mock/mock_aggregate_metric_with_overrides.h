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

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"
#include "public/cpio/utils/metric_aggregation/src/aggregate_metric.h"

namespace google::scp::cpio {
class MockAggregateMetricOverrides : public AggregateMetric {
 public:
  explicit MockAggregateMetricOverrides(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<MetricClientInterface>& metric_client,
      const std::shared_ptr<MetricDefinition>& metric_info,
      const core::TimeDuration aggregation_time_duration_ms,
      const std::shared_ptr<std::vector<std::string>>& event_list = nullptr)
      : AggregateMetric(async_executor, metric_client, metric_info,
                        aggregation_time_duration_ms, event_list) {
    // For mocking purposes, we should be able to accept incoming metric even if
    // the mock is not Run().
    can_accept_incoming_increments_ = true;
  }

  std::function<void()> run_metric_push_mock;

  std::function<void(int64_t counter,
                     const std::shared_ptr<MetricTag>& metric_tag)>
      metric_push_handler_mock;

  std::function<core::ExecutionResult()> schedule_metric_push_mock;

  core::ExecutionResult Run() noexcept { return AggregateMetric::Run(); }

  size_t GetCounter(const std::string& event_code = std::string()) {
    if (event_code.empty()) {
      return AggregateMetric::counter_.load();
    }

    auto event = AggregateMetric::event_counters_.find(event_code);
    if (event != AggregateMetric::event_counters_.end()) {
      return event->second.load();
    }
    return 0;
  }

  std::shared_ptr<MetricTag> GetMetricTag(const std::string& event_code) {
    auto event = AggregateMetric::event_tags_.find(event_code);
    if (event != AggregateMetric::event_tags_.end()) {
      return event->second;
    }
    return nullptr;
  }

  void MetricPushHandler(
      int64_t counter,
      const std::shared_ptr<MetricTag>& metric_tag = nullptr) noexcept {
    if (metric_push_handler_mock) {
      return metric_push_handler_mock(counter, metric_tag);
    }
    return AggregateMetric::MetricPushHandler(counter, metric_tag);
  }

  void RunMetricPush() noexcept {
    if (run_metric_push_mock) {
      return run_metric_push_mock();
    }
    return AggregateMetric::RunMetricPush();
  }

  core::ExecutionResult ScheduleMetricPush() noexcept {
    if (schedule_metric_push_mock) {
      return schedule_metric_push_mock();
    }
    return AggregateMetric::ScheduleMetricPush();
  }
};

}  // namespace google::scp::cpio
