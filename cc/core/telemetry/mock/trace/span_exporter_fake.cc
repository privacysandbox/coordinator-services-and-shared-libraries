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

#include "cc/core/telemetry/mock/trace/span_exporter_fake.h"

#include <utility>

#include "cc/core/telemetry/src/common/trace/trace_utils.h"

namespace privacy_sandbox::pbs_common {

SpanExporterFake::SpanExporterFake() noexcept = default;

std::unique_ptr<opentelemetry::sdk::trace::Recordable>
SpanExporterFake::MakeRecordable() noexcept {
  return std::make_unique<opentelemetry::sdk::trace::SpanData>();
}

opentelemetry::sdk::common::ExportResult SpanExporterFake::Export(
    const opentelemetry::nostd::span<
        std::unique_ptr<opentelemetry::sdk::trace::Recordable>>&
        spans) noexcept {
  if (is_shutdown_) {
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  for (std::unique_ptr<opentelemetry::sdk::trace::Recordable>& recordable :
       spans) {
    auto span_data = std::unique_ptr<opentelemetry::sdk::trace::SpanData>(
        dynamic_cast<opentelemetry::sdk::trace::SpanData*>(
            recordable.release()));

    if (span_data != nullptr) {
      // Extract the trace ID.
      std::string trace_id = GetTraceIdString(span_data->GetTraceId());

      // Store the span data in the map by trace ID.
      collected_spans_[trace_id].emplace_back(span_data.get());

      // Store the span data in the map by span name.
      std::string span_name = std::string(span_data->GetName());
      collected_spans_by_name_[span_name].emplace_back(span_data.get());

      // Store span data as unique ptr to keep the ownership.
      span_data_.push_back(std::move(span_data));
    }
  }

  return opentelemetry::sdk::common::ExportResult::kSuccess;
}

bool SpanExporterFake::ForceFlush(
    std::chrono::microseconds /*timeout*/) noexcept {
  // Since this is a mock, we do nothing here.
  return true;
}

bool SpanExporterFake::Shutdown(
    std::chrono::microseconds /*timeout*/) noexcept {
  is_shutdown_ = true;
  return true;
}

std::vector<const opentelemetry::sdk::trace::SpanData*>
SpanExporterFake::GetSpansForTraceId(absl::string_view trace_id) const {
  if (auto it = collected_spans_.find(std::string(trace_id));
      it != collected_spans_.end()) {
    return it->second;
  }
  return {};
}

absl::flat_hash_map<std::string,
                    std::vector<const opentelemetry::sdk::trace::SpanData*>>
SpanExporterFake::GetCollectedSpansByTraceId() const {
  return collected_spans_;
}

std::vector<const opentelemetry::sdk::trace::SpanData*>
SpanExporterFake::GetSpansForSpanName(absl::string_view span_name) const {
  auto it = collected_spans_by_name_.find(span_name);

  if (it != collected_spans_by_name_.end()) {
    return it->second;
  }
  return {};
}

absl::flat_hash_map<std::string,
                    std::vector<const opentelemetry::sdk::trace::SpanData*>>
SpanExporterFake::GetCollectedSpansBySpanName() const {
  return collected_spans_by_name_;
}

}  // namespace privacy_sandbox::pbs_common
