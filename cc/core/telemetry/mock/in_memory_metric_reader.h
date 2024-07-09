#pragma once

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/synchronization/notification.h"
#include "core/telemetry/mock/in_memory_metric_exporter.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

namespace google::scp::core {
class InMemoryMetricReader : public opentelemetry::sdk::metrics::MetricReader {
 public:
  explicit InMemoryMetricReader(
      std::unique_ptr<core::InMemoryMetricExporter> exporter);

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType instrument_type)
      const noexcept override;

  core::InMemoryMetricExporter& exporter() const;
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> GetExportedData()
      const;

 private:
  bool OnForceFlush(std::chrono::microseconds timeout) noexcept override;

  bool OnShutDown(std::chrono::microseconds timeout) noexcept override;

  void OnInitialized() noexcept override;

  std::unique_ptr<core::InMemoryMetricExporter> exporter_;
};
}  // namespace google::scp::core
