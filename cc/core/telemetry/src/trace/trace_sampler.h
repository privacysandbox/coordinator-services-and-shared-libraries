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

#ifndef CC_CORE_TELEMETRY_SRC_TRACE_TRACE_SAMPLER_H_
#define CC_CORE_TELEMETRY_SRC_TRACE_TRACE_SAMPLER_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/trace/span.h"

namespace google::scp::core {

/**
 * @brief TraceSampler class for custom sampling of traces.
 *
 * The TraceSampler class implements a custom trace sampling strategy based on
 * a head-based sampling ratio and parent span context.
 */
class TraceSampler : public opentelemetry::sdk::trace::Sampler {
 public:
  /**
   * @brief Constructor for TraceSampler.
   *
   * Initializes the TraceSampler with a specified sampling ratio.
   * The sampling ratio should be in the range [0, 1], where 1.0 means
   * sampling all traces, and 0.0 means sampling no traces.
   *
   * @param sampling_ratio The ratio of traces to sample (default: 1.0).
   */
  explicit TraceSampler(double sampling_ratio = 1.0);

  /**
   * @brief Decides whether a trace should be sampled.
   *
   * Implements the sampling logic to determine if a given trace should be
   * sampled based on the provided parameters such as trace ID, span name,
   * span kind, attributes, and links.
   *
   * @param parent_context The parent span context of the current trace.
   * @param trace_id The unique identifier for the trace.
   * @param name The name of the span.
   * @param span_kind The kind of span (e.g., internal, server, client).
   * @param attributes Attributes associated with the span.
   * @param links Links to other spans.
   * @return The sampling result indicating whether to sample the trace.
   */
  opentelemetry::sdk::trace::SamplingResult ShouldSample(
      const opentelemetry::trace::SpanContext& parent_context,
      opentelemetry::trace::TraceId trace_id,
      opentelemetry::nostd::string_view name,
      opentelemetry::trace::SpanKind span_kind,
      const opentelemetry::common::KeyValueIterable& attributes,
      const opentelemetry::trace::SpanContextKeyValueIterable& links) noexcept
      override;

  /**
   * @brief Provides a description of the sampler.
   *
   * Returns a description of the sampler, which
   * helps to understand the sampler's configuration and behavior.
   *
   * @return A string view describing the sampler.
   */
  [[nodiscard]] opentelemetry::nostd::string_view GetDescription()
      const noexcept override;

 private:
  /**
   * @brief Calculates a sampling threshold based on the ratio.
   *
   * Converts the sampling ratio in [0, 1] to a threshold value in [0,
   * UINT64_MAX] used for determining whether a trace should be sampled.
   *
   * @param ratio The sampling ratio.
   * @return The calculated threshold value.
   */
  uint64_t CalculateThreshold(double ratio) noexcept;

  /**
   * @brief Converts a TraceId to a threshold value.
   *
   * Uses the TraceId to generate a threshold value for sampling decisions.
   *
   * @param trace_id The TraceId for which to calculate the threshold.
   * @return The calculated threshold value based on the TraceId.
   */
  uint64_t CalculateThresholdFromBuffer(
      const opentelemetry::trace::TraceId& trace_id) noexcept;

  // The custom ratio for head-based sampling.
  double sampling_ratio_;

  // Sampler description.
  std::string description_;
};

}  // namespace google::scp::core
#endif  // CC_CORE_TELEMETRY_SRC_TRACE_TRACE_SAMPLER_H_
