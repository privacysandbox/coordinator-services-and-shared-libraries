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

#include <functional>
#include <memory>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "cpio/client_providers/metric_client_provider/src/utils/timeseries_metric.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockTimeSeriesMetricOverrides : public TimeSeriesMetric {
 public:
  explicit MockTimeSeriesMetricOverrides(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<MetricClientProviderInterface>& metric_client,
      const std::shared_ptr<MetricDefinition>& metric_info,
      const core::TimeDuration& time_duration)
      : TimeSeriesMetric(async_executor, metric_client, metric_info,
                         time_duration) {}

  std::function<core::ExecutionResult(const std::shared_ptr<MetricValue>&,
                                      const std::shared_ptr<MetricTag>&)>
      metric_push_handler_mock;
  std::function<void()> run_metric_push_mock;
  std::function<core::ExecutionResult()> schedule_metric_push_mock;

  core::ExecutionResult Run() noexcept { return TimeSeriesMetric::Run(); }

  uint64_t GetCounter() { return TimeSeriesMetric::counter_; }

  uint64_t GetAccumulativeTime() {
    return TimeSeriesMetric::accumulative_time_;
  }

  void Push(const std::shared_ptr<TimeEvent>& time_event) noexcept override {
    return TimeSeriesMetric::Push(time_event);
  }

  void Increment() noexcept { TimeSeriesMetric::Increment(); }

  void IncrementBy(uint64_t incrementer) noexcept {
    TimeSeriesMetric::IncrementBy(incrementer);
  }

  void Decrement() noexcept { TimeSeriesMetric::Decrement(); }

  void DecrementBy(uint64_t decrementer) noexcept {
    TimeSeriesMetric::DecrementBy(decrementer);
  }

  core::ExecutionResult MetricPushHandler(
      const std::shared_ptr<MetricValue>& value,
      const std::shared_ptr<MetricTag>& metric_tag) noexcept {
    if (metric_push_handler_mock) {
      return metric_push_handler_mock(value, metric_tag);
    }
    return TimeSeriesMetric::MetricPushHandler(value, metric_tag);
  };

  void RunMetricPush() noexcept {
    if (run_metric_push_mock) {
      return run_metric_push_mock();
    }
    return TimeSeriesMetric::RunMetricPush();
  };

  core::ExecutionResult ScheduleMetricPush() noexcept {
    if (schedule_metric_push_mock) {
      return schedule_metric_push_mock();
    }
    return TimeSeriesMetric::ScheduleMetricPush();
  }
};

}  // namespace google::scp::cpio::client_providers::mock
