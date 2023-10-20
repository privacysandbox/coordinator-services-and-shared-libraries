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

#include "metric_utils.h"

using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::cpio {

void MetricUtils::GetPutMetricsRequest(
    shared_ptr<cmrt::sdk::metric_service::v1::PutMetricsRequest>&
        record_metric_request,
    const shared_ptr<MetricDefinition>& metric_info,
    const shared_ptr<MetricValue>& metric_value,
    const shared_ptr<MetricTag>& metric_tag) noexcept {
  auto metric = record_metric_request->add_metrics();
  metric->set_value(*metric_value);
  auto final_name = (metric_tag && metric_tag->update_name)
                        ? *metric_tag->update_name
                        : *metric_info->name;
  metric->set_name(final_name);
  auto final_unit = (metric_tag && metric_tag->update_unit)
                        ? *metric_tag->update_unit
                        : *metric_info->unit;
  metric->set_unit(
      client_providers::MetricClientUtils::ConvertToMetricUnitProto(
          final_unit));

  // Adds the labels from metric_info and additional_labels.
  auto labels = metric->mutable_labels();
  if (metric_info->labels) {
    for (const auto& label : *metric_info->labels) {
      labels->insert(protobuf::MapPair(label.first, label.second));
    }
  }
  if (metric_tag && metric_tag->additional_labels) {
    for (const auto& label : *metric_tag->additional_labels) {
      labels->insert(protobuf::MapPair(label.first, label.second));
    }
  }
  *metric->mutable_timestamp() = protobuf::util::TimeUtil::GetCurrentTime();
  if (metric_info->name_space != nullptr) {
    record_metric_request->set_metric_namespace(*metric_info->name_space);
  }
}

/**
 * @brief Registers a simple metric with MetricClient.
 *
 * @param async_executor
 * @param metric_client
 * @param metric_name_str Name of the metric
 * @param metric_label_component Component Name where the metric is emitted
 * @param metric_label_method Method Name where the metric is emitted
 * @param metric_unit_type unit type
 * @return shared_ptr<SimpleMetricInterface>
 */
shared_ptr<SimpleMetricInterface> MetricUtils::RegisterSimpleMetric(
    const shared_ptr<core::AsyncExecutorInterface>& async_executor,
    const shared_ptr<MetricClientInterface>& metric_client,
    const string& metric_name_str, const string& metric_label_component_str,
    const string& metric_label_method_str,
    MetricUnit metric_unit_type) noexcept {
  auto metric_name = make_shared<MetricName>(metric_name_str);
  auto metric_unit = make_shared<MetricUnit>(metric_unit_type);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(metric_label_component_str,
                              metric_label_method_str);
  metric_info->labels =
      make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  return make_shared<SimpleMetric>(async_executor, metric_client, metric_info);
}

/**
 * @brief Registers a aggretate metric with MetricClient.
 *
 * @param async_executor
 * @param metric_client
 * @param metric_name_str Name of the metric
 * @param metric_label_component Component Name where the metric is emitted
 * @param metric_label_method Method Name where the metric is emitted
 * @param metric_unit_type unit type
 * @param metric_event_labels Dimension labels of the metric
 * @param aggregated_metric_interval_ms Aggregation interval
 * @return shared_ptr<AggregateMetricInterface>
 */
shared_ptr<AggregateMetricInterface> MetricUtils::RegisterAggregateMetric(
    const shared_ptr<core::AsyncExecutorInterface>& async_executor,
    const shared_ptr<MetricClientInterface>& metric_client,
    const string& metric_name_str, const string& metric_label_component,
    const string& metric_label_method, MetricUnit metric_unit_type,
    vector<string> metric_event_labels,
    size_t aggregated_metric_interval_ms) noexcept {
  auto metric_name = make_shared<MetricName>(metric_name_str);
  auto metric_unit = make_shared<MetricUnit>(metric_unit_type);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(metric_label_component, metric_label_method);
  metric_info->labels =
      make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  return make_shared<AggregateMetric>(
      async_executor, metric_client, metric_info, aggregated_metric_interval_ms,
      make_shared<vector<string>>(metric_event_labels));
}
}  // namespace google::scp::cpio
