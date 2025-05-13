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

#include "cc/core/telemetry/src/trace/trace_sampler.h"

#include <cmath>
#include <cstring>

namespace privacy_sandbox::pbs_common {

TraceSampler::TraceSampler(double sampling_ratio)
    : sampling_ratio_(sampling_ratio) {
  if (sampling_ratio > 1.0) {
    sampling_ratio_ = 1.0;
  } else if (sampling_ratio < 0.0) {
    sampling_ratio_ = 0.0;
  }

  description_ = "TraceSampler{" + std::to_string(sampling_ratio_) + "}";
}

opentelemetry::sdk::trace::SamplingResult TraceSampler::ShouldSample(
    const opentelemetry::trace::SpanContext& parent_context,
    opentelemetry::trace::TraceId trace_id,
    opentelemetry::nostd::string_view /*name*/,
    opentelemetry::trace::SpanKind /*span_kind*/,
    const opentelemetry::common::KeyValueIterable& /*attributes*/,
    const opentelemetry::trace::
        SpanContextKeyValueIterable& /*links*/) noexcept {
  // Head-based sampling: if there's no parent context, we sample based on a
  // custom ratio.
  if (!parent_context.IsValid()) {
    if (CalculateThresholdFromBuffer(trace_id) <=
        CalculateThreshold(sampling_ratio_)) {
      return {
          .decision = opentelemetry::sdk::trace::Decision::RECORD_AND_SAMPLE,
          .attributes = nullptr,
          .trace_state = {}};
    } else {
      return {.decision = opentelemetry::sdk::trace::Decision::DROP,
              .attributes = nullptr,
              .trace_state = {}};
    }
  }

  // If parent span exists and is sampled, continue to sample.
  if (parent_context.IsSampled()) {
    return {opentelemetry::sdk::trace::Decision::RECORD_AND_SAMPLE, nullptr,
            parent_context.trace_state()};
  }

  // Otherwise, drop.
  return {opentelemetry::sdk::trace::Decision::DROP, nullptr,
          parent_context.trace_state()};
}

opentelemetry::nostd::string_view TraceSampler::GetDescription()
    const noexcept {
  return description_;
}

uint64_t TraceSampler::CalculateThreshold(double ratio) noexcept {
  if (ratio <= 0.0) return 0;
  if (ratio >= 1.0) return UINT64_MAX;

  // We can't directly return ratio * UINT64_MAX.
  //
  // UINT64_MAX is (2^64)-1, but as a double rounds up to 2^64.
  // For probabilities >= 1-(2^-54), the product wraps to zero!
  // Instead, calculate the high and low 32 bits separately.
  // https://github.com/open-telemetry/opentelemetry-cpp/blob/23818a7105c2565ff0a07580a585d10ec3dc8db4/sdk/src/trace/samplers/trace_id_ratio.cc#L74
  const double product = UINT32_MAX * ratio;
  double hi_bits, lo_bits = ldexp(modf(product, &hi_bits), 32) + product;
  return (static_cast<uint64_t>(hi_bits) << 32) +
         static_cast<uint64_t>(lo_bits);
}

uint64_t TraceSampler::CalculateThresholdFromBuffer(
    const opentelemetry::trace::TraceId& trace_id) noexcept {
  static_assert(opentelemetry::trace::TraceId::kSize >= 8,
                "TraceID must be at least 8 bytes long.");

  uint64_t res = 0;
  std::memcpy(&res, &trace_id, 8);

  // Convert the trace ID to a ratio in [0, 1] by normalizing it.
  double ratio = static_cast<double>(res) / static_cast<double>(UINT64_MAX);

  return CalculateThreshold(ratio);
}

}  // namespace privacy_sandbox::pbs_common
