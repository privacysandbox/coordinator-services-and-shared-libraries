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

#include <string>

#include "opentelemetry/trace/trace_id.h"

namespace privacy_sandbox::pbs_common {

std::string GetTraceIdString(const opentelemetry::trace::TraceId& tid) {
  char trace_id[opentelemetry::trace::TraceId::kSize * 2] = {0};
  tid.ToLowerBase16(trace_id);
  return std::string(trace_id, opentelemetry::trace::TraceId::kSize * 2);
}

}  // namespace privacy_sandbox::pbs_common
