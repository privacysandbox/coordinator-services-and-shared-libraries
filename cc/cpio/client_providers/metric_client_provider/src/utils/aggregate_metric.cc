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

#include "aggregate_metric.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

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
using google::scp::core::errors::SC_CUSTOMIZED_METRIC_EVENT_CODE_NOT_EXIST;
using google::scp::core::errors::SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricValue;
using google::scp::cpio::client_providers::MetricClientProviderInterface;
using std::make_shared;
using std::move;
using std::mutex;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::milliseconds;

namespace google::scp::cpio::client_providers {
AggregateMetric::AggregateMetric(
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<MetricClientProviderInterface>& metric_client,
    const shared_ptr<MetricDefinition>& metric_info, TimeDuration time_duration,
    const shared_ptr<vector<string>>& event_code_list,
    const string& event_code_label_key)
    : async_executor_(async_executor),
      metric_client_(metric_client),
      metric_info_(metric_info),
      time_duration_(time_duration),
      counter_(0),
      is_running_(false) {
  if (event_code_list) {
    for (const auto& event_code : *event_code_list) {
      MetricLabels labels;
      labels[event_code_label_key] = event_code;
      auto tag = make_shared<MetricTag>(nullptr, nullptr,
                                        make_shared<MetricLabels>(labels));
      event_counters_[event_code] = 0;
      event_tags_[event_code] = tag;
    }
  }
}

ExecutionResult AggregateMetric::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AggregateMetric::Run() noexcept {
  is_running_ = true;
  return ScheduleMetricPush();
}

ExecutionResult AggregateMetric::Stop() noexcept {
  sync_mutex_.lock();
  is_running_ = false;
  sync_mutex_.unlock();

  current_cancellation_callback_();
  return SuccessExecutionResult();
}

ExecutionResult AggregateMetric::Increment(const string& event_code) noexcept {
  if (event_code.empty()) {
    counter_++;
    return SuccessExecutionResult();
  }

  auto event = event_counters_.find(event_code);
  if (event == event_counters_.end()) {
    return FailureExecutionResult(SC_CUSTOMIZED_METRIC_EVENT_CODE_NOT_EXIST);
  }
  event->second++;
  return SuccessExecutionResult();
}

void AggregateMetric::MetricPushHandler(
    int64_t value, const shared_ptr<MetricTag>& metric_tag) noexcept {
  auto metric_value = make_shared<MetricValue>(to_string(value));

  auto record_metric_request = make_shared<PutMetricsRequest>();
  MetricUtils::GetPutMetricsRequest(record_metric_request, metric_info_,
                                    metric_value, metric_tag);

  AsyncContext<PutMetricsRequest, PutMetricsResponse> record_metric_context(
      move(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& outcome) {
        if (!outcome.result.Successful()) {
          // TODO: Create an alert or reschedule
        }
      });

  auto execution_result = metric_client_->PutMetrics(record_metric_context);
  if (!execution_result.Successful()) {
    // TODO: Create an alert or reschedule
  }
  return;
}

void AggregateMetric::RunMetricPush() noexcept {
  auto value = counter_.exchange(0);
  if (value > 0) {
    MetricPushHandler(value);
  }

  for (auto event = event_counters_.begin(); event != event_counters_.end();
       ++event) {
    auto value = event->second.exchange(0);
    if (value > 0) {
      MetricPushHandler(value, event_tags_[event->first]);
    }
  }
  return;
}

ExecutionResult AggregateMetric::ScheduleMetricPush() noexcept {
  Timestamp next_push_time = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                              milliseconds(time_duration_))
                                 .count();

  if (!is_running_) {
    return FailureExecutionResult(SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE);
  }

  auto execution_result = async_executor_->ScheduleFor(
      [this]() {
        ScheduleMetricPush();
        RunMetricPush();
      },
      next_push_time, current_cancellation_callback_);

  if (!execution_result.Successful()) {
    return FailureExecutionResult(SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE);
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
