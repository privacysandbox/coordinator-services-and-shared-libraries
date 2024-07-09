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

namespace google::scp::core {
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
}  // namespace google::scp::core
