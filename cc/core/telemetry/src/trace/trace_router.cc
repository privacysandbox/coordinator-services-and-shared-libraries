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

#include "cc/core/telemetry/src/trace/trace_router.h"

#include <memory>
#include <string>
#include <utility>

#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/telemetry/src/common/telemetry_configuration.h"
#include "cc/core/telemetry/src/trace/trace_sampler.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"

namespace privacy_sandbox::pbs_common {
namespace {

/**
 * @brief Creates a SpanProcessor based on configuration and SpanExporter.
 *
 * This method creates and returns a SpanProcessor configured with
 * values from the configuration provider and the provided SpanExporter.
 *
 * @param config_provider A reference to the configuration provider.
 * @param exporter A unique pointer to the SpanExporter.
 * @return A unique pointer to the SpanProcessor.
 */
std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> CreateSpanProcessor(
    ConfigProviderInterface& config_provider,
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter) {
  // Fetch the export interval in milliseconds from the config provider.
  int32_t export_interval_millis =
      GetConfigValue(std::string(kOtelTraceBatchExportIntervalMsecKey),
                     kOtelTraceBatchExportIntervalMsecValue, config_provider);

  // Fetch the maximum span buffer size from the config provider.
  int32_t max_queue_size =
      GetConfigValue(std::string(kOtelTraceMaxSpanBufferKey),
                     kOtelTraceMaxSpanBufferValue, config_provider);

  // Fetch the maximum export batch size from the config provider.
  int32_t max_export_batch_size =
      GetConfigValue(std::string(kOtelTraceMaxExportBatchSizeKey),
                     kOtelTraceMaxExportBatchSizeValue, config_provider);

  opentelemetry::sdk::trace::BatchSpanProcessorOptions options;
  options.max_queue_size = max_queue_size;
  options.schedule_delay_millis =
      std::chrono::milliseconds(export_interval_millis);
  options.max_export_batch_size = max_export_batch_size;

  return opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
      std::move(exporter), options);
}

/**
 * @brief Creates a SpanProcessor based on configuration and SpanExporter.
 *
 * This method creates and returns a SpanProcessor configured with
 * values from the configuration provider and the provided SpanExporter.
 *
 * @param config_provider A reference to the configuration provider.
 * @param exporter A unique pointer to the SpanExporter.
 * @return A unique pointer to the SpanProcessor.
 */
std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateSpanSampler(
    ConfigProviderInterface& config_provider) {
  // Fetch the sampling ratio from the config provider.
  double sampling_ratio =
      GetConfigValue(std::string(kOtelTraceSamplingRatioKey),
                     kOtelTraceSamplingRatioValue, config_provider);

  return std::make_unique<TraceSampler>(sampling_ratio);
}
}  // namespace

TraceRouter::TraceRouter(
    ConfigProviderInterface& config_provider,
    const opentelemetry::sdk::resource::Resource& resource,
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> span_exporter) {
  SetupTraceRouter(
      resource, CreateSpanProcessor(config_provider, std::move(span_exporter)),
      CreateSpanSampler(config_provider));
}

void TraceRouter::SetupTraceRouter(
    const opentelemetry::sdk::resource::Resource& resource,
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> span_processor,
    std::unique_ptr<opentelemetry::sdk::trace::Sampler> span_sampler) {
  std::unique_ptr<opentelemetry::sdk::trace::TracerProvider> trace_provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(span_processor), resource, std::move(span_sampler));

  opentelemetry::trace::Provider::SetTracerProvider(std::move(trace_provider));
}

}  // namespace privacy_sandbox::pbs_common
