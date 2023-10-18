/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(SC_ROMA_DISPATCHER, 0x0C00)
DEFINE_ERROR_CODE(
    SC_ROMA_DISPATCHER_DISPATCH_DISALLOWED_DUE_TO_ONGOING_LOAD,
    SC_ROMA_DISPATCHER, 0x0001,
    "Dispatch is disallowed since a load/broadcast request is ongoing.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_DISPATCHER_DISPATCH_DISALLOWED_DUE_TO_CAPACITY,
                  SC_ROMA_DISPATCHER, 0x0002,
                  "Dispatch is disallowed since the number of unfinished "
                  "requests is at capacity.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
