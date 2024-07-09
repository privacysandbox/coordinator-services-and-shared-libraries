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

#include "core/telemetry/src/metric/metric_router.h"

#include <string>
#include <utility>

#include "core/telemetry/src/common/telemetry_configuration.h"
#include "core/telemetry/src/metric/otlp_grpc_authed_metric_exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"

namespace google::scp::core {

MetricRouter::MetricRouter(
    std::shared_ptr<ConfigProviderInterface> config_provider,
    opentelemetry::sdk::resource::Resource resource,
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter) {
  SetupMetricRouter(std::move(resource),
                    CreatePeriodicReader(config_provider, std::move(exporter)));
}

void MetricRouter::SetupMetricRouter(
    opentelemetry::sdk::resource::Resource resource,
    std::shared_ptr<opentelemetry::sdk::metrics::MetricReader> metric_reader) {
  auto views = opentelemetry::sdk::metrics::ViewRegistryFactory::Create();

  auto meter_context = opentelemetry::sdk::metrics::MeterContextFactory::Create(
      std::move(views), std::move(resource));
  meter_context->AddMetricReader(metric_reader);

  auto u_provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::move(meter_context));

  opentelemetry::metrics::Provider::SetMeterProvider(std::move(u_provider));
}

std::shared_ptr<opentelemetry::sdk::metrics::MetricReader>
MetricRouter::CreatePeriodicReader(
    std::shared_ptr<ConfigProviderInterface> config_provider,
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter) {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      reader_options;

  int32_t export_interval =
      GetConfigValue(std::string(kOtelMetricExportIntervalMsecKey),
                     kOtelMetricExportIntervalMsecValue, *config_provider);
  reader_options.export_interval_millis =
      std::chrono::milliseconds(export_interval);

  int32_t export_timeout =
      GetConfigValue(std::string(kOtelMetricExportTimeoutMsecKey),
                     kOtelMetricExportTimeoutMsecValue, *config_provider);
  reader_options.export_timeout_millis =
      std::chrono::milliseconds(export_timeout);

  return std::make_shared<
      opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
      std::move(exporter), reader_options);
}

}  // namespace google::scp::core
