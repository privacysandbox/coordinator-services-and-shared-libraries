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

#include "cc/core/telemetry/mock/trace/span_sampler_fake.h"

namespace privacy_sandbox::pbs_common {

opentelemetry::sdk::trace::SamplingResult SpanSamplerFake::ShouldSample(
    const opentelemetry::trace::SpanContext& /*parent_context*/,
    opentelemetry::trace::TraceId /*trace_id*/,
    opentelemetry::nostd::string_view /*name*/,
    opentelemetry::trace::SpanKind /*span_kind*/,
    const opentelemetry::common::KeyValueIterable& /*attributes*/,
    const opentelemetry::trace::
        SpanContextKeyValueIterable& /*links*/) noexcept {
  // Always return RECORD_AND_SAMPLE to indicate sampling
  return {opentelemetry::sdk::trace::Decision::RECORD_AND_SAMPLE, nullptr, {}};
}

opentelemetry::nostd::string_view SpanSamplerFake::GetDescription()
    const noexcept {
  return description_;
}
}  // namespace privacy_sandbox::pbs_common
