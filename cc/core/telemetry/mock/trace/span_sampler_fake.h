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
#ifndef CC_CORE_TELEMETRY_MOCK_TRACE_TRACE_SAMPLER_FAKE_H_
#define CC_CORE_TELEMETRY_MOCK_TRACE_TRACE_SAMPLER_FAKE_H_

#include <string>

#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/trace/span.h"

namespace privacy_sandbox::pbs_common {
class SpanSamplerFake : public opentelemetry::sdk::trace::Sampler {
 public:
  // Constructor initializes the sampler with a default description.
  SpanSamplerFake() : description_("SpanSamplerFake{AlwaysSample}") {}

  // Implements the main sampling logic.
  // Always returns a sampling result indicating the span should be sampled.
  opentelemetry::sdk::trace::SamplingResult ShouldSample(
      const opentelemetry::trace::SpanContext& parent_context,
      opentelemetry::trace::TraceId trace_id,
      opentelemetry::nostd::string_view name,
      opentelemetry::trace::SpanKind span_kind,
      const opentelemetry::common::KeyValueIterable& attributes,
      const opentelemetry::trace::SpanContextKeyValueIterable& links) noexcept
      override;

  // Returns the description of the sampler.
  [[nodiscard]] opentelemetry::nostd::string_view GetDescription()
      const noexcept override;

 private:
  // Sampler description for debugging.
  std::string description_;
};
}  // namespace privacy_sandbox::pbs_common
#endif  // CC_CORE_TELEMETRY_MOCK_TRACE_TRACE_SAMPLER_FAKE_H_
