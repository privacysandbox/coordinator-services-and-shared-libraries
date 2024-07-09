//  Copyright 2024 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "core/telemetry/mock/in_memory_metric_exporter.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

#include "opentelemetry/exporters/ostream/common_utils.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/aggregation/histogram_aggregation.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk_config.h"

namespace google::scp::core {

InMemoryMetricExporter::InMemoryMetricExporter(
    bool is_otel_print_data_to_console_enabled, std::ostream& sout,
    opentelemetry::sdk::metrics::AggregationTemporality
        aggregation_temporality) noexcept
    : is_otel_print_data_to_console_enabled_(
          is_otel_print_data_to_console_enabled),
      sout_(sout),
      is_shutdown_(false),
      aggregation_temporality_(aggregation_temporality) {
  o_stream_metric_exporter_ =
      std::make_unique<opentelemetry::exporter::metrics::OStreamMetricExporter>(
          sout_);
}

opentelemetry::sdk::metrics::AggregationTemporality
InMemoryMetricExporter::GetAggregationTemporality(
    opentelemetry::sdk::metrics::InstrumentType /* instrument_type */)
    const noexcept {
  return aggregation_temporality_;
}

/**
 * Metric Data: this can be referred as a metric. It contains information about
 * the instrumentation and the list of data points
 *
 * Scope Metrics: List of Metric Data (metric). This is at the meter level. We
 * create meters for each service
 *
 * Resource Metrics: List of Scope Metrics (contains all the data)
 *
 */
opentelemetry::sdk::common::ExportResult InMemoryMetricExporter::Export(
    const opentelemetry::sdk::metrics::ResourceMetrics& data) noexcept {
  if (is_shutdown()) {
    OTEL_INTERNAL_LOG_ERROR("[OStream Metric] Exporting "
                            << data.scope_metric_data_.size()
                            << " records(s) failed, exporter is shutdown");
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  // No data to export
  // This happens when the reader and the exporter time don't overlap or if
  // there is no data to send For the testing purpose, we want to make sure that
  // we are exporting the data
  if (data.scope_metric_data_.empty()) {
    return opentelemetry::sdk::common::ExportResult::kSuccess;
  }
  data_.push_back(data);

  if (is_otel_print_data_to_console_enabled_) {
    PrintData(data);
  }

  return opentelemetry::sdk::common::ExportResult::kSuccess;
}

void InMemoryMetricExporter::PrintData(
    const opentelemetry::sdk::metrics::ResourceMetrics& data) {
  o_stream_metric_exporter_->Export(data);
}

bool InMemoryMetricExporter::ForceFlush(
    std::chrono::microseconds /* timeout */) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return true;
}

bool InMemoryMetricExporter::Shutdown(
    std::chrono::microseconds /* timeout */) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  is_shutdown_ = true;
  return true;
}

bool InMemoryMetricExporter::is_shutdown() const noexcept {
  return is_shutdown_;
}

std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
InMemoryMetricExporter::data() const {
  return data_;
}

}  // namespace google::scp::core
