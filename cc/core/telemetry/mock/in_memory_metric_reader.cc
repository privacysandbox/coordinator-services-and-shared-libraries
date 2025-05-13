// Copyright 2025 Google LLC
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

#include "in_memory_metric_reader.h"

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/aggregation/histogram_aggregation.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace privacy_sandbox::pbs_common {
InMemoryMetricReader::InMemoryMetricReader(
    std::unique_ptr<InMemoryMetricExporter> exporter)
    : exporter_{std::move(exporter)} {}

opentelemetry::sdk::metrics::AggregationTemporality
InMemoryMetricReader::GetAggregationTemporality(
    opentelemetry::sdk::metrics::InstrumentType instrument_type)
    const noexcept {
  return exporter_->GetAggregationTemporality(instrument_type);
}

void InMemoryMetricReader::OnInitialized() noexcept {}

bool InMemoryMetricReader::OnForceFlush(
    std::chrono::microseconds timeout) noexcept {
  return Collect(
      [this](opentelemetry::sdk::metrics::ResourceMetrics& metric_data) {
        return this->exporter_->Export(metric_data) ==
               opentelemetry::sdk::common::ExportResult::kSuccess;
      });
}

bool InMemoryMetricReader::OnShutDown(
    std::chrono::microseconds timeout) noexcept {
  return exporter_->Shutdown(timeout);
}

InMemoryMetricExporter& InMemoryMetricReader::exporter() const {
  return *exporter_;
}

std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
InMemoryMetricReader::GetExportedData() const {
  return exporter_->data();
}
}  // namespace privacy_sandbox::pbs_common
