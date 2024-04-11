// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cc/pbs/front_end_service/src/metric_initialization.h"

#include "cc/public/cpio/utils/metric_aggregation/interface/type_def.h"
#include "cc/public/cpio/utils/metric_aggregation/src/aggregate_metric.h"

namespace google::scp::pbs {
namespace {

using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::ExecutionResultOr;
using ::google::scp::cpio::AggregateMetric;
using ::google::scp::cpio::AggregateMetricInterface;
using ::google::scp::cpio::kCountSecond;
using ::google::scp::cpio::MetricClientInterface;
using ::google::scp::cpio::MetricDefinition;
using ::google::scp::cpio::MetricLabels;
using ::google::scp::cpio::MetricLabelsBase;
using ::google::scp::cpio::MetricName;
using ::google::scp::cpio::MetricUnit;

ExecutionResultOr<std::shared_ptr<AggregateMetricInterface>>
RegisterAggregateMetric(
    absl::string_view name, absl::string_view phase,
    const std::shared_ptr<AsyncExecutorInterface>& async_executor,
    const std::shared_ptr<MetricClientInterface>& metric_client,
    const core::TimeDuration aggregated_metric_interval_ms) noexcept {
  std::shared_ptr<MetricName> metric_name = std::make_shared<MetricName>(name);
  std::shared_ptr<MetricUnit> metric_unit =
      std::make_shared<MetricUnit>(kCountSecond);
  std::shared_ptr<MetricDefinition> metric_info =
      std::make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(kMetricLabelFrontEndService, std::string(phase));

  metric_info->labels =
      std::make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  std::vector<std::string> labels_list = {kMetricLabelValueOperator,
                                          kMetricLabelValueCoordinator};
  return std::make_shared<AggregateMetric>(
      async_executor, metric_client, metric_info, aggregated_metric_interval_ms,
      std::make_shared<std::vector<std::string>>(labels_list),
      kMetricLabelKeyReportingOrigin);
}

}  // namespace

ExecutionResultOr<MetricsMap> MetricInitializationImplementation::Initialize(
    std::shared_ptr<AsyncExecutorInterface> async_executor,
    std::shared_ptr<MetricClientInterface> metric_client,
    const core::TimeDuration aggregated_metric_interval_ms) const noexcept {
  MetricsMap metrics_map;

  for (absl::string_view method_name : kMetricInitializationMethodNames) {
    for (absl::string_view metric_name : kMetricInitializationMetricNames) {
      ExecutionResultOr<std::shared_ptr<AggregateMetricInterface>>
          metric_instance_or = RegisterAggregateMetric(
              metric_name, method_name, async_executor, metric_client,
              aggregated_metric_interval_ms);
      RETURN_IF_FAILURE(metric_instance_or.result());
      metrics_map[method_name][metric_name] = metric_instance_or.value();
    }
  }

  return metrics_map;
}

}  // namespace google::scp::pbs
