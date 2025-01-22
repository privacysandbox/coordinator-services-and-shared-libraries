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

#include "cc/core/telemetry/src/common/trace/trace_utils.h"

#include <gtest/gtest.h>

#include "opentelemetry/trace/trace_id.h"

namespace google::scp::core {
namespace {
TEST(TraceUtilsTest, GetTraceIdStringForValidTraceId) {
  // Define a test TraceId (128-bit, represented as 16 bytes).
  uint8_t trace_id_bytes[opentelemetry::trace::TraceId::kSize] = {
      0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
      0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  opentelemetry::trace::TraceId trace_id(trace_id_bytes);

  // Validate the result.
  EXPECT_EQ(GetTraceIdString(trace_id), "123456789abcdef01122334455667788");
}

TEST(TraceUtilsTest, GetTraceIdStringForEmptyTraceId) {
  // Define an empty TraceId (all zeroes).
  uint8_t trace_id_bytes[opentelemetry::trace::TraceId::kSize] = {0};
  opentelemetry::trace::TraceId trace_id(trace_id_bytes);

  // Validate the result.
  EXPECT_EQ(GetTraceIdString(trace_id), "00000000000000000000000000000000");
}
}  // namespace
}  // namespace google::scp::core
