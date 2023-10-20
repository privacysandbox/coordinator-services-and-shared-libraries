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

#include "partition_metrics_wrapper.h"

#include <string>
#include <utility>
#include <vector>

#include "core/interface/async_executor_interface.h"
#include "pbs/interface/metrics_def.h"
#include "public/cpio/utils/metric_aggregation/src/aggregate_metric.h"
#include "public/cpio/utils/metric_aggregation/src/metric_utils.h"
#include "public/cpio/utils/metric_aggregation/src/simple_metric.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::PartitionId;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::AggregateMetric;
using google::scp::cpio::AggregateMetricInterface;
using google::scp::cpio::kCountUnit;
using google::scp::cpio::kMillisecondsUnit;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricLabelsBase;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::MetricUtils;
using google::scp::cpio::MetricValue;
using google::scp::cpio::SimpleMetric;
using google::scp::cpio::SimpleMetricInterface;
using google::scp::cpio::TimeEvent;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_lock;
using std::unique_ptr;
using std::vector;

namespace google::scp::pbs {

ExecutionResult PartitionMetricsWrapper::Init() {
  load_duration_metric_ = MetricUtils::RegisterSimpleMetric(
      async_executor_, metric_client_, kMetricNamePartitionLoadDurationMs,
      kMetricComponentNameAndPartitionNamePrefixForPartitionLeaseSink +
          ToString(partition_id_),
      kMetricMethodPartitionLoad, kMillisecondsUnit);
  unload_duration_metric_ = MetricUtils::RegisterSimpleMetric(
      async_executor_, metric_client_, kMetricNamePartitionUnloadDurationMs,
      kMetricComponentNameAndPartitionNamePrefixForPartitionLeaseSink +
          ToString(partition_id_),
      kMetricMethodPartitionUnload, kMillisecondsUnit);
  lease_event_metrics_ = MetricUtils::RegisterAggregateMetric(
      async_executor_, metric_client_, kMetricNamePartitionLeaseEvent,
      kMetricComponentNameAndPartitionNamePrefixForPartitionLeaseSink +
          ToString(partition_id_),
      kMetricMethodPartitionLeaseEvent, kCountUnit,
      {kMetricEventPartitionLeaseRenewedCount},
      metric_aggregation_interval_milliseconds_);

  RETURN_IF_FAILURE(load_duration_metric_->Init());
  RETURN_IF_FAILURE(unload_duration_metric_->Init());
  RETURN_IF_FAILURE(lease_event_metrics_->Init());
  return SuccessExecutionResult();
}

ExecutionResult PartitionMetricsWrapper::Run() {
  RETURN_IF_FAILURE(load_duration_metric_->Run());
  RETURN_IF_FAILURE(unload_duration_metric_->Run());
  RETURN_IF_FAILURE(lease_event_metrics_->Run());
  return SuccessExecutionResult();
}

ExecutionResult PartitionMetricsWrapper::Stop() {
  RETURN_IF_FAILURE(load_duration_metric_->Stop());
  RETURN_IF_FAILURE(unload_duration_metric_->Stop());
  RETURN_IF_FAILURE(lease_event_metrics_->Stop());
  return SuccessExecutionResult();
}

void PartitionMetricsWrapper::OnPartitionUnloaded(size_t duration) {
  unload_duration_metric_->Push(make_shared<MetricValue>(to_string(duration)));
}

void PartitionMetricsWrapper::OnPartitionLoaded(size_t duration) {
  load_duration_metric_->Push(make_shared<MetricValue>(to_string(duration)));
}

void PartitionMetricsWrapper::OnLeaseRenewed() {
  lease_event_metrics_->Increment(kMetricEventPartitionLeaseRenewedCount);
}

}  // namespace google::scp::pbs
