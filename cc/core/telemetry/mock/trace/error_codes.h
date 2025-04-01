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
#ifndef CC_CORE_TELEMETRY_MOCK_TRACE_ERROR_CODES_H_
#define CC_CORE_TELEMETRY_MOCK_TRACE_ERROR_CODES_H_

#include "cc/core/interface/errors.h"

namespace google::scp::core {

REGISTER_COMPONENT_CODE(SC_PBS_TRACE_MOCK, 0x0530)

DEFINE_ERROR_CODE(
    SC_TRACE_MOCK_COULD_NOT_EXPORT_DATA, SC_PBS_TRACE_MOCK, 0x0001,
    "Trace exporter could not export/force-flush the data",
    privacy_sandbox::pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_SPAN_PROCESSOR_COULD_NOT_EXPORT_DATA, SC_PBS_TRACE_MOCK, 0x0002,
    "SpanProcessor could not export the data",
    privacy_sandbox::pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core
#endif  // CC_CORE_TELEMETRY_MOCK_TRACE_ERROR_CODES_H_
