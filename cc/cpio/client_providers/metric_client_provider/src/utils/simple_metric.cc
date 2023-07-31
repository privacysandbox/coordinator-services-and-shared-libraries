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

#include "simple_metric.h"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "metric_utils.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
// using google::scp::cpio::MetricValue;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using std::make_shared;
using std::move;
using std::shared_ptr;

namespace google::scp::cpio::client_providers {
ExecutionResult SimpleMetric::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult SimpleMetric::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult SimpleMetric::Stop() noexcept {
  return SuccessExecutionResult();
}

void SimpleMetric::RunMetricPush(
    const shared_ptr<PutMetricsRequest> record_metric_request) noexcept {
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
}

void SimpleMetric::Push(const shared_ptr<MetricValue>& metric_value,
                        const shared_ptr<MetricTag>& metric_tag) noexcept {
  auto record_metric_request = make_shared<PutMetricsRequest>();
  MetricUtils::GetPutMetricsRequest(record_metric_request, metric_info_,
                                    metric_value, metric_tag);
  async_executor_->Schedule(
      [this, record_metric_request]() { RunMetricPush(record_metric_request); },
      AsyncPriority::Normal);
}

}  // namespace google::scp::cpio::client_providers
