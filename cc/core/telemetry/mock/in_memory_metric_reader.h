/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/synchronization/notification.h"
#include "cc/core/telemetry/mock/in_memory_metric_exporter.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

namespace privacy_sandbox::pbs_common {
class InMemoryMetricReader : public opentelemetry::sdk::metrics::MetricReader {
 public:
  explicit InMemoryMetricReader(
      std::unique_ptr<InMemoryMetricExporter> exporter);

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType instrument_type)
      const noexcept override;

  InMemoryMetricExporter& exporter() const;
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> GetExportedData()
      const;

 private:
  bool OnForceFlush(std::chrono::microseconds timeout) noexcept override;

  bool OnShutDown(std::chrono::microseconds timeout) noexcept override;

  void OnInitialized() noexcept override;

  std::unique_ptr<InMemoryMetricExporter> exporter_;
};
}  // namespace privacy_sandbox::pbs_common
