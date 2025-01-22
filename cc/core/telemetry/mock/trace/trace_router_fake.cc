// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "cc/core/telemetry/mock/trace/trace_router_fake.h"

#include <memory>
#include <string>
#include <vector>

#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/telemetry/mock/trace/error_codes.h"
#include "cc/core/telemetry/mock/trace/span_exporter_fake.h"
#include "cc/core/telemetry/mock/trace/span_processor_fake.h"
#include "cc/core/telemetry/mock/trace/span_sampler_fake.h"

namespace google::scp::core {
namespace {
inline constexpr absl::string_view kTraceRouterMock = "TraceRouterFake";

/**
 * @brief Creates a mock SpanProcessor.
 * @param config_provider The config provider.
 * @param exporter The SpanExporter to use.
 * @return A unique pointer to the mock SpanProcessor.
 */
std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> CreateSpanProcessor(
    std::shared_ptr<opentelemetry::sdk::trace::SpanExporter> exporter) {
  return std::make_unique<SpanProcessorFake>(exporter);
}

/**
 * @brief Creates a mock SpanSampler.
 * @param config_provider The config provider.
 * @return A unique pointer to the mock SpanSampler.
 */
std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateSpanSampler() {
  return std::make_unique<SpanSamplerFake>();
}

/**
 * @brief Creates a mock SpanExporter.
 * @return A shared pointer to the mock SpanExporter.
 */
std::shared_ptr<SpanExporterFake> CreateSpanExporter() {
  return std::make_shared<SpanExporterFake>();
}
}  // namespace

TraceRouterFake::TraceRouterFake() : span_exporter_(CreateSpanExporter()) {
  SetupTraceRouter(opentelemetry::sdk::resource::Resource::GetDefault(),
                   CreateSpanProcessor(span_exporter_), CreateSpanSampler());
}

SpanExporterFake& TraceRouterFake::GetSpanExporter() const {
  return *span_exporter_;
}

absl::flat_hash_map<std::string,
                    std::vector<const opentelemetry::sdk::trace::SpanData*>>
TraceRouterFake::GetExportedTraces() const {
  if (span_exporter_->ForceFlush()) {
    return span_exporter_->GetCollectedSpansByTraceId();
  }
  return {};
}

std::vector<const opentelemetry::sdk::trace::SpanData*>
TraceRouterFake::GetSpansForTrace(absl::string_view trace_id) const {
  if (span_exporter_->ForceFlush()) {
    return span_exporter_->GetSpansForTraceId(trace_id);
  }
  return {};
}

absl::flat_hash_map<std::string,
                    std::vector<const opentelemetry::sdk::trace::SpanData*>>
TraceRouterFake::GetExportedSpansBySpanName() const {
  if (span_exporter_->ForceFlush()) {
    return span_exporter_->GetCollectedSpansBySpanName();
  }
  return {};
}

std::vector<const opentelemetry::sdk::trace::SpanData*>
TraceRouterFake::GetSpansForSpanName(absl::string_view span_name) const {
  if (span_exporter_->ForceFlush()) {
    return span_exporter_->GetSpansForSpanName(span_name);
  }
  return {};
}

}  // namespace google::scp::core
