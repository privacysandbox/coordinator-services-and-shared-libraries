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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/metrics_def.h"
#include "core/telemetry/src/common/telemetry_configuration.h"
#include "core/telemetry/src/metric/otlp_grpc_authed_metric_exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/view/view.h"
#include "opentelemetry/sdk/metrics/view/view_factory.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"

namespace google::scp::core {

inline constexpr absl::string_view kMetricRouter = "MetricRouter";

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

std::shared_ptr<opentelemetry::metrics::Meter> MetricRouter::GetOrCreateMeter(
    absl::string_view service_name, absl::string_view version,
    absl::string_view schema_url) {
  absl::MutexLock lock(&metric_mutex_);
  if (auto it = meters_.find(service_name); it != meters_.end()) {
    return it->second;
  }
  auto meter = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
      service_name, version, schema_url);
  meters_[service_name] = meter;
  return meter;
}

std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>
MetricRouter::GetOrCreateSyncInstrument(
    absl::string_view metric_name,
    absl::AnyInvocable<
        std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
        instrument_factory) {
  absl::MutexLock lock(&metric_mutex_);
  if (auto it = synchronous_instruments_.find(metric_name);
      it != synchronous_instruments_.end()) {
    return it->second;
  }

  // If not found, create the instrument using the factory.
  auto new_instrument = instrument_factory();
  synchronous_instruments_[metric_name] = new_instrument;
  return new_instrument;
}

std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
MetricRouter::GetOrCreateObservableInstrument(
    absl::string_view metric_name,
    absl::AnyInvocable<
        std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
        instrument_factory) {
  absl::MutexLock lock(&metric_mutex_);
  if (auto it = asynchronous_instruments_.find(metric_name);
      it != asynchronous_instruments_.end()) {
    return it->second;
  }

  // If not found, create the instrument using the factory.
  auto new_instrument = instrument_factory();
  asynchronous_instruments_[metric_name] = new_instrument;
  return new_instrument;
}

ExecutionResult MetricRouter::CreateHistogramViewForInstrument(
    absl::string_view metric_name, absl::string_view view_name,
    InstrumentType instrument_type, const std::vector<double>& boundaries,
    absl::string_view version, absl::string_view schema,
    absl::string_view view_description, absl::string_view unit) {
  auto meter_selector =
      std::make_unique<opentelemetry::sdk::metrics::MeterSelector>(
          std::string(metric_name), std::string(version), std::string(schema));

  auto histogram_aggregation_config = std::make_shared<
      opentelemetry::sdk::metrics::HistogramAggregationConfig>();
  histogram_aggregation_config->boundaries_ = boundaries;

  std::unique_ptr<opentelemetry::sdk::metrics::View> view =
      opentelemetry::sdk::metrics::ViewFactory::Create(
          std::string(view_name), std::string(view_description),
          std::string(unit),
          opentelemetry::sdk::metrics::AggregationType::kHistogram,
          histogram_aggregation_config);

  std::unique_ptr<opentelemetry::sdk::metrics::InstrumentSelector>
      instrument_selector;

  switch (instrument_type) {
    case InstrumentType::kCounter:
      instrument_selector =
          std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
              opentelemetry::sdk::metrics::InstrumentType::kCounter,
              std::string(metric_name), std::string(unit));
      break;
    case InstrumentType::kHistogram:
      instrument_selector =
          std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
              opentelemetry::sdk::metrics::InstrumentType::kHistogram,
              std::string(metric_name), std::string(unit));
      break;
    case InstrumentType::kGauge:
      instrument_selector =
          std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
              opentelemetry::sdk::metrics::InstrumentType::kObservableGauge,
              std::string(metric_name), std::string(unit));
      break;
  }

  auto meter_provider = opentelemetry::metrics::Provider::GetMeterProvider();
  if (!dynamic_cast<opentelemetry::metrics::NoopMeterProvider*>(
          meter_provider.get())) {
    std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> sdk_provider =
        std::dynamic_pointer_cast<opentelemetry::sdk::metrics::MeterProvider>(
            meter_provider);

    sdk_provider->AddView(std::move(instrument_selector),
                          std::move(meter_selector), std::move(view));
  } else {
    auto execution_result =
        FailureExecutionResult(SC_TELEMETRY_METER_PROVIDER_NOT_INITIALIZED);
    SCP_ERROR(kMetricRouter, google::scp::core::common::kZeroUuid,
              execution_result, "[OTLP Metric Router] Meter Provider is NOOP.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

}  // namespace google::scp::core
