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

#include "cc/core/telemetry/src/metric/metric_router.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/telemetry/src/common/telemetry_configuration.h"
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
using ::privacy_sandbox::pbs_common::ConfigProviderInterface;

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

  std::unique_ptr<opentelemetry::sdk::metrics::MeterProvider> meter_provider =
      opentelemetry::sdk::metrics::MeterProviderFactory::Create(
          std::move(meter_context));

  opentelemetry::metrics::Provider::SetMeterProvider(std::move(meter_provider));
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
  std::string service(service_name);
  auto [it, inserted] = meters_.insert(
      {service, std::shared_ptr<opentelemetry::metrics::Meter>()});
  if (inserted) {
    it->second = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
        service, version, schema_url);
  }
  return it->second;
}

std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>
MetricRouter::GetOrCreateSyncInstrument(
    absl::string_view metric_name,
    absl::AnyInvocable<
        std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
        instrument_factory) {
  absl::MutexLock lock(&metric_mutex_);
  std::string metric(metric_name);
  auto [it, inserted] = synchronous_instruments_.insert(
      {metric,
       std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()});
  if (inserted) {
    it->second = instrument_factory();
  }
  return it->second;
}

std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
MetricRouter::GetOrCreateObservableInstrument(
    absl::string_view metric_name,
    absl::AnyInvocable<
        std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
        instrument_factory) {
  absl::MutexLock lock(&metric_mutex_);
  std::string metric(metric_name);
  auto [it, inserted] = asynchronous_instruments_.insert(
      {metric,
       std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()});
  if (inserted) {
    it->second = instrument_factory();
  }
  return it->second;
}

ExecutionResult MetricRouter::CreateViewForInstrument(
    absl::string_view meter_name, absl::string_view instrument_name,
    opentelemetry::sdk::metrics::InstrumentType instrument_type,
    opentelemetry::sdk::metrics::AggregationType aggregation_type,
    absl::Span<const double> boundaries, absl::string_view version,
    absl::string_view schema_url, absl::string_view view_description,
    absl::string_view unit) {
  auto meter_selector =
      std::make_unique<opentelemetry::sdk::metrics::MeterSelector>(
          std::string(meter_name), std::string(version),
          std::string(schema_url));

  auto histogram_aggregation_config = std::make_shared<
      opentelemetry::sdk::metrics::HistogramAggregationConfig>();
  histogram_aggregation_config->boundaries_ =
      std::vector<double>(boundaries.begin(), boundaries.end());

  std::unique_ptr<opentelemetry::sdk::metrics::View> view =
      opentelemetry::sdk::metrics::ViewFactory::Create(
          std::string(instrument_name), std::string(view_description),
          std::string(unit), aggregation_type, histogram_aggregation_config);

  std::unique_ptr<opentelemetry::sdk::metrics::InstrumentSelector>
      instrument_selector =
          std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
              instrument_type, std::string(instrument_name), std::string(unit));

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
