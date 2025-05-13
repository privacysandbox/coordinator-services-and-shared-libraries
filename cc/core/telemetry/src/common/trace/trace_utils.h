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

#ifndef CC_CORE_TELEMETRY_SRC_COMMON_TRACE_TRACE_UTILS_H_
#define CC_CORE_TELEMETRY_SRC_COMMON_TRACE_TRACE_UTILS_H_

#include <string>

#include "opentelemetry/trace/trace_id.h"

namespace privacy_sandbox::pbs_common {

// Converts the TraceId from a Span to a std::string
std::string GetTraceIdString(const opentelemetry::trace::TraceId& tid);

}  // namespace privacy_sandbox::pbs_common
#endif  // CC_CORE_TELEMETRY_SRC_COMMON_TRACE_TRACE_UTILS_H_
