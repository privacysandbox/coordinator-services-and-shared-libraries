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

#include <memory>
#include <string>

#include "cc/core/interface/config_provider_interface.h"
#include "external/io_opentelemetry_cpp/api/_virtual_includes/api/opentelemetry/metrics/meter.h"
#include "external/io_opentelemetry_cpp/api/_virtual_includes/api/opentelemetry/metrics/meter_provider.h"
#include "external/io_opentelemetry_cpp/api/_virtual_includes/api/opentelemetry/metrics/observer_result.h"
#include "external/io_opentelemetry_cpp/api/_virtual_includes/api/opentelemetry/metrics/sync_instruments.h"
#include "external/io_opentelemetry_cpp/api/_virtual_includes/api/opentelemetry/nostd/shared_ptr.h"
#include "external/io_opentelemetry_cpp/sdk/_virtual_includes/headers/opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "external/io_opentelemetry_cpp/sdk/_virtual_includes/headers/opentelemetry/sdk/metrics/push_metric_exporter.h"

/**
 * @brief MetricRouter class for handling OpenTelemetry metric.
 * The MetricRouter class is responsible for managing an OpenTelemetry
 * MeterProvider and provides access to it.
 */
namespace google::scp::core {

class MetricRouter {
 public:
  // For testing only
  MetricRouter() = default;
  explicit MetricRouter(
      std::shared_ptr<ConfigProviderInterface> config_provider,
      std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
          exporter);

  virtual std::shared_ptr<opentelemetry::metrics::MeterProvider>
  meter_provider() const;
  virtual opentelemetry::sdk::metrics::PushMetricExporter& exporter() const;

 private:
  void SetUpMeterProvider(
      std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
          exporter);
  std::shared_ptr<opentelemetry::metrics::MeterProvider> meter_provider_;
  std::shared_ptr<ConfigProviderInterface> config_provider_;
  opentelemetry::sdk::metrics::PushMetricExporter* exporter_;
  std::shared_ptr<opentelemetry::sdk::metrics::PeriodicExportingMetricReader>
      periodic_exporting_metric_reader_;
};
}  // namespace google::scp::core
