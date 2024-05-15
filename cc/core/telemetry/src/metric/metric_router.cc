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
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"

namespace google::scp::core {

MetricRouter::MetricRouter(
    std::shared_ptr<ConfigProviderInterface> config_provider,
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter)
    : config_provider_(config_provider) {
  SetUpMeterProvider(std::move(exporter));
}

void MetricRouter::SetUpMeterProvider(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter) {
  // exporter_ will not escape and become dangling because the ownership of the
  // unique ptr will finally stay with the MeterProvider which remains in scope
  // globally
  exporter_ = exporter.get();
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      reader_options;

  int32_t export_interval =
      GetConfigValue(std::string(kOtelMetricExportIntervalMsecKey),
                     kOtelMetricExportIntervalMsecValue, *config_provider_);
  reader_options.export_interval_millis =
      std::chrono::milliseconds(export_interval);

  int32_t export_timeout =
      GetConfigValue(std::string(kOtelMetricExportTimeoutMsecKey),
                     kOtelMetricExportTimeoutMsecValue, *config_provider_);
  reader_options.export_timeout_millis =
      std::chrono::milliseconds(export_timeout);

  periodic_exporting_metric_reader_ = std::make_shared<
      opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
      std::move(exporter), reader_options);

  auto context = opentelemetry::sdk::metrics::MeterContextFactory::Create();
  context->AddMetricReader(periodic_exporting_metric_reader_);

  auto u_provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::move(context));
  meter_provider_ = std::move(u_provider);

  opentelemetry::metrics::Provider::SetMeterProvider(meter_provider_);
}

std::shared_ptr<opentelemetry::metrics::MeterProvider>
MetricRouter::meter_provider() const {
  return meter_provider_;
}

opentelemetry::sdk::metrics::PushMetricExporter& MetricRouter::exporter()
    const {
  return *exporter_;
}

}  // namespace google::scp::core
