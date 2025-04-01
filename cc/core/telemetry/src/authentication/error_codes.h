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
#pragma once

#include "cc/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::scp::core {

REGISTER_COMPONENT_CODE(SC_PBS_TELEMETRY_AUTHENTICATION, 0x0235)

DEFINE_ERROR_CODE(
    SC_TELEMETRY_AUTHENTICATION_ID_TOKEN_FETCH_FAILED,
    SC_PBS_TELEMETRY_AUTHENTICATION, 0x0001, "Id token fetch failed",
    privacy_sandbox::pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core
