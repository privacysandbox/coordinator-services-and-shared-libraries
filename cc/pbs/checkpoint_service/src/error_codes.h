/*
 * Copyright 2022 Google LLC
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

#include "cc/core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

REGISTER_COMPONENT_CODE(SC_PBS_CHECKPOINT_SERVICE, 0x0106)

DEFINE_ERROR_CODE(SC_PBS_CHECKPOINT_SERVICE_NO_LOGS_TO_PROCESS,
                  SC_PBS_CHECKPOINT_SERVICE, 0x0001, "No logs to process.",
                  HttpStatusCode::NO_CONTENT)

DEFINE_ERROR_CODE(SC_PBS_CHECKPOINT_SERVICE_IS_ALREADY_RUNNING,
                  SC_PBS_CHECKPOINT_SERVICE, 0x0002,
                  "Checkpoint service is already running.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_CHECKPOINT_SERVICE_IS_ALREADY_STOPPED,
                  SC_PBS_CHECKPOINT_SERVICE, 0x0003,
                  "Checkpoint service is already stopped.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_PBS_CHECKPOINT_SERVICE_INVALID_LAST_PERSISTED_CHECKPOINT_ID,
    SC_PBS_CHECKPOINT_SERVICE, 0x0004,
    "Last persisted checkpoint Id is invalid. No checkpoint has been persisted "
    "since the start service startup.",
    HttpStatusCode::NO_CONTENT)
}  // namespace google::scp::core::errors
