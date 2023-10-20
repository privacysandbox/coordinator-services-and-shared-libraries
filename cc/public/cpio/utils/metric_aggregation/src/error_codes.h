// Copyright 2022 Google LLC
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

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
/// Registers component code as 0x020D for customized metric.
REGISTER_COMPONENT_CODE(SC_CUSTOMIZED_METRIC, 0x020D)

DEFINE_ERROR_CODE(SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE,
                  SC_CUSTOMIZED_METRIC, 0x0001, "Cannot schedule metric push",
                  HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_CUSTOMIZED_METRIC_EVENT_CODE_NOT_EXIST,
                  SC_CUSTOMIZED_METRIC, 0x0002, "Event code cannot be found",
                  HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_CUSTOMIZED_METRIC_NOT_RUNNING, SC_CUSTOMIZED_METRIC,
                  0x0003, "Metric not running", HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_CUSTOMIZED_METRIC_ALREADY_RUNNING, SC_CUSTOMIZED_METRIC,
                  0x0004, "Metric already running", HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_CUSTOMIZED_METRIC_CANNOT_INCREMENT_WHEN_NOT_RUNNING,
                  SC_CUSTOMIZED_METRIC, 0x0005,
                  "Metric cannot be incremented when it is not running",
                  HttpStatusCode::CONFLICT)

}  // namespace google::scp::core::errors
