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

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/trace_id.h"

namespace google::scp::core::test {
namespace {

// Helper function to create a TraceId with a specific uint64_t value.
opentelemetry::trace::TraceId CreateTraceId(uint64_t id_value) {
  uint8_t buffer[opentelemetry::trace::TraceId::kSize] = {};
  std::memcpy(buffer, &id_value, sizeof(id_value));
  return opentelemetry::trace::TraceId(buffer);
}

// Helper function to create a SpanId with a specific uint64_t value.
opentelemetry::trace::SpanId CreateSpanId(uint64_t id_value) {
  uint8_t buffer[opentelemetry::trace::SpanId::kSize] = {};
  std::memcpy(buffer, &id_value, sizeof(id_value));
  return opentelemetry::trace::SpanId(buffer);
}

class TraceSamplerTest : public ::testing::Test {
 protected:
  TraceSamplerTest()
      : valid_parent_context_(CreateTraceId(0xFFFFFFFFFFFFFFFF),
                              CreateSpanId(0x00000000FFFFFFFA),
                              opentelemetry::trace::TraceFlags{
                                  opentelemetry::trace::TraceFlags::kIsSampled},
                              /*is_remote=*/true),
        invalid_parent_context_(
            opentelemetry::trace::TraceId(), opentelemetry::trace::SpanId(),
            opentelemetry::trace::TraceFlags{0}, /*is_remote=*/false),
        unsampled_parent_context_(
            CreateTraceId(0xFFFFFFFFFFFFFFFF), CreateSpanId(0x00000000FFFFFFFA),
            opentelemetry::trace::TraceFlags{
                opentelemetry::trace::TraceFlags::kIsRandom},
            true) {}

  // Members for setting up the test scenarios.
  opentelemetry::trace::SpanContext valid_parent_context_;
  opentelemetry::trace::SpanContext invalid_parent_context_;
  opentelemetry::trace::SpanContext unsampled_parent_context_;
};

// Test that the sampler correctly handles edge cases for sampling ratio.
TEST_F(TraceSamplerTest, ConstructorHandlesSamplingRatioEdgeCases) {
  TraceSampler sampler_high(1.5);  // Greater than 1.0
  EXPECT_EQ(std::string(sampler_high.GetDescription()),
            "TraceSampler{1.000000}");

  TraceSampler sampler_low(-0.5);  // Less than 0.0
  EXPECT_EQ(sampler_low.GetDescription(), "TraceSampler{0.000000}");
}

// Test that the sampler correctly samples or drops based on the sampling ratio.
TEST_F(TraceSamplerTest, ShouldSampleNoParentContext) {
  TraceSampler sampler(0.5);
  opentelemetry::trace::TraceId trace_id = CreateTraceId(0x00000000FFFFFFFF);
  opentelemetry::sdk::trace::SamplingResult result = sampler.ShouldSample(
      /*parent_context=*/invalid_parent_context_,
      /*trace_id=*/trace_id,
      /*name=*/"",
      /*span_kind=*/opentelemetry::trace::SpanKind::kInternal,
      /*attributes=*/opentelemetry::common::NoopKeyValueIterable(),
      /*links=*/opentelemetry::trace::NullSpanContext());
  EXPECT_EQ(result.decision,
            opentelemetry::sdk::trace::Decision::RECORD_AND_SAMPLE);

  trace_id = CreateTraceId(0xFFFFFFFFFFFFFFFF);
  result = sampler.ShouldSample(
      /*parent_context=*/invalid_parent_context_,
      /*trace_id=*/trace_id,
      /*name=*/"",
      /*span_kind=*/opentelemetry::trace::SpanKind::kInternal,
      /*attributes=*/opentelemetry::common::NoopKeyValueIterable(),
      /*links=*/opentelemetry::trace::NullSpanContext());
  EXPECT_EQ(result.decision, opentelemetry::sdk::trace::Decision::DROP);
}

// Test that the sampler continues to sample when the parent context is sampled.
TEST_F(TraceSamplerTest, ShouldSampleWithSampledParentContext) {
  // Low ratio, but parent context is sampled.
  TraceSampler sampler(0.1);
  opentelemetry::trace::TraceId trace_id = valid_parent_context_.trace_id();

  opentelemetry::sdk::trace::SamplingResult result = sampler.ShouldSample(
      /*parent_context=*/valid_parent_context_,
      /*trace_id=*/trace_id,
      /*name=*/"",
      /*span_kind=*/opentelemetry::trace::SpanKind::kInternal,
      /*attributes=*/opentelemetry::common::NoopKeyValueIterable(),
      /*links=*/opentelemetry::trace::NullSpanContext());
  EXPECT_EQ(result.decision,
            opentelemetry::sdk::trace::Decision::RECORD_AND_SAMPLE);
}

// Test that the sampler does not sample when the parent context is not sampled.
TEST_F(TraceSamplerTest, ShouldNotSampleWithNotSampledParentContext) {
  // High ratio, but parent context is not sampled.
  TraceSampler sampler(0.9);
  opentelemetry::trace::TraceId trace_id = unsampled_parent_context_.trace_id();

  opentelemetry::sdk::trace::SamplingResult result = sampler.ShouldSample(
      /*parent_context=*/unsampled_parent_context_, /*trace_id=*/trace_id,
      /*name=*/"", /*span_kind=*/opentelemetry::trace::SpanKind::kInternal,
      /*attributes=*/opentelemetry::common::NoopKeyValueIterable(),
      /*links=*/opentelemetry::trace::NullSpanContext());
  EXPECT_EQ(result.decision, opentelemetry::sdk::trace::Decision::DROP);
}

}  // namespace
}  // namespace google::scp::core::test
