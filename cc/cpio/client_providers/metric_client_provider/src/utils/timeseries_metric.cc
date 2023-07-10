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

#include "timeseries_metric.h"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"
#include "metric_utils.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::Timestamp;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::MetricValue;
using std::atomic;
using std::bind;
using std::make_shared;
using std::move;
using std::mutex;
using std::shared_ptr;
using std::to_string;
using std::chrono::milliseconds;

static constexpr char kTimeSeriesMetricType[] = "Type";
static constexpr char kTimeSeriesMetricAverageExecutionTime[] =
    "AverageExecutionTime";
static constexpr char kTimeSeriesMetricRequestReceived[] = "RequestReceived";

namespace google::scp::cpio::client_providers {
TimeSeriesMetric::TimeSeriesMetric(
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<MetricClientProviderInterface>& metric_client,
    const shared_ptr<MetricDefinition>& metric_info,
    const TimeDuration& time_duration = 1000)
    : async_executor_(async_executor),
      metric_client_(metric_client),
      metric_info_(metric_info),
      time_duration_(time_duration),
      counter_(0),
      accumulative_time_(0),
      is_running_(false) {
  cumulative_time_tag_ = make_shared<MetricTag>();
  cumulative_time_tag_->update_unit =
      make_shared<MetricUnit>(kMillisecondsUnit);
  MetricLabels time_additional_labels;
  time_additional_labels[kTimeSeriesMetricType] =
      kTimeSeriesMetricAverageExecutionTime;
  cumulative_time_tag_->additional_labels =
      make_shared<MetricLabels>(time_additional_labels);

  counter_tag_ = make_shared<MetricTag>();
  counter_tag_->update_unit = make_shared<MetricUnit>(kCountUnit);
  MetricLabels counter_additional_labels;
  counter_additional_labels[kTimeSeriesMetricType] =
      kTimeSeriesMetricRequestReceived;
  counter_tag_->additional_labels =
      make_shared<MetricLabels>(counter_additional_labels);
}

ExecutionResult TimeSeriesMetric::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TimeSeriesMetric::Run() noexcept {
  is_running_ = true;
  return ScheduleMetricPush();
}

ExecutionResult TimeSeriesMetric::Stop() noexcept {
  sync_mutex_.lock();
  is_running_ = false;
  sync_mutex_.unlock();

  current_cancellation_callback_();
  return SuccessExecutionResult();
}

void TimeSeriesMetric::Increment() noexcept {
  IncrementBy(1);
}

void TimeSeriesMetric::IncrementBy(uint64_t incrementer) noexcept {
  counter_ += incrementer;
}

void TimeSeriesMetric::Decrement() noexcept {
  DecrementBy(1);
}

void TimeSeriesMetric::DecrementBy(uint64_t decrementer) noexcept {
  counter_ -= decrementer;
}

void TimeSeriesMetric::Push(
    const std::shared_ptr<TimeEvent>& time_event) noexcept {
  accumulative_time_ += time_event->diff_time;
  Increment();
}

ExecutionResult TimeSeriesMetric::MetricPushHandler(
    const shared_ptr<MetricValue>& value,
    const shared_ptr<MetricTag>& metric_tag) noexcept {
  auto record_metric_request = make_shared<PutMetricsRequest>();

  MetricUtils::GetPutMetricsRequest(record_metric_request, metric_info_, value,
                                    metric_tag);

  AsyncContext<PutMetricsRequest, PutMetricsResponse> record_metric_context(
      move(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& outcome) {
        if (!outcome.result.Successful()) {
          // TODO: Create an alert or reschedule
        }
      });

  return metric_client_->PutMetrics(record_metric_context);
}

void TimeSeriesMetric::RunMetricPush() noexcept {
  // To avoid invalid value metric push, skips the push when counter_ is 0.
  if (counter_ == 0) {
    ScheduleMetricPush();
    return;
  }

  auto metric_counter = make_shared<MetricValue>(to_string(counter_));
  auto execution_result = MetricPushHandler(metric_counter, counter_tag_);
  if (!execution_result.Successful()) {
    // TODO: Create an alert or reschedule
  }

  auto average_time =
      make_shared<MetricValue>(to_string(accumulative_time_ / counter_));
  execution_result = MetricPushHandler(average_time, cumulative_time_tag_);
  if (!execution_result.Successful()) {
    // TODO: Create an alert or reschedule
  }

  counter_ = 0;
  accumulative_time_ = 0;

  if (execution_result.Successful()) {
    ScheduleMetricPush();
  }
  return;
}

ExecutionResult TimeSeriesMetric::ScheduleMetricPush() noexcept {
  Timestamp next_push_time = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                              milliseconds(time_duration_))
                                 .count();

  sync_mutex_.lock();
  if (!is_running_) {
    sync_mutex_.unlock();
    return FailureExecutionResult(SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE);
  }

  auto execution_result = async_executor_->ScheduleFor(
      [this]() { RunMetricPush(); }, next_push_time,
      current_cancellation_callback_);
  if (!execution_result.Successful()) {
    // TODO: Create an alert
  }

  sync_mutex_.unlock();
  return execution_result;
}

}  // namespace google::scp::cpio::client_providers
