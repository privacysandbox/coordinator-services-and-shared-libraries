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

#pragma once

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/synchronization/notification.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

namespace google::scp::core {

class InMemoryMetricExporter final
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  explicit InMemoryMetricExporter(
      bool is_otel_print_data_to_console_enabled = false,
      std::ostream& sout = std::cout,
      opentelemetry::sdk::metrics::AggregationTemporality
          aggregation_temporality = opentelemetry::sdk::metrics::
              AggregationTemporality::kCumulative) noexcept;

  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::sdk::metrics::ResourceMetrics& data) noexcept
      override;

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType instrument_type)
      const noexcept override;

  bool ForceFlush(std::chrono::microseconds timeout =
                      (std::chrono::microseconds::max)()) noexcept override;

  bool Shutdown(std::chrono::microseconds timeout =
                    (std::chrono::microseconds::max)()) noexcept override;

  // Utility method to help debugging the exported data
  void PrintData(const opentelemetry::sdk::metrics::ResourceMetrics& data);
  // Returns the exported data
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data() const;

  bool is_shutdown() const noexcept;

 private:
  // Boolean that specifies whether metrics should be printed to the console.
  bool is_otel_print_data_to_console_enabled_;

  std::ostream& sout_;
  bool is_shutdown_;
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
      data_;  // exported data
  mutable std::mutex mutex_;
  opentelemetry::sdk::metrics::AggregationTemporality aggregation_temporality_;
  std::unique_ptr<opentelemetry::exporter::metrics::OStreamMetricExporter>
      o_stream_metric_exporter_;
};

}  // namespace google::scp::core
