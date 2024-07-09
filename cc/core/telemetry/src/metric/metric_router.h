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
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource.h"

/**
 * @brief MetricRouter class for handling OpenTelemetry metric.
 * The MetricRouter class is responsible for managing an OpenTelemetry
 * MeterProvider and provides access to it.
 */
namespace google::scp::core {

class MetricRouter {
 public:
  // Create a MetricRouter with a Periodic Reader, given a Resource and a
  // cloud-specific Exporter
  explicit MetricRouter(
      std::shared_ptr<ConfigProviderInterface> config_provider,
      opentelemetry::sdk::resource::Resource resource,
      std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
          exporter);

 protected:
  MetricRouter() = default;

  // For subclasses used in testing, to allow custom Resource and Reader
  void SetupMetricRouter(
      opentelemetry::sdk::resource::Resource resource,
      std::shared_ptr<opentelemetry::sdk::metrics::MetricReader> metric_reader);

 private:
  static std::shared_ptr<opentelemetry::sdk::metrics::MetricReader>
  CreatePeriodicReader(
      std::shared_ptr<ConfigProviderInterface> config_provider,
      std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
          exporter);
};
}  // namespace google::scp::core
