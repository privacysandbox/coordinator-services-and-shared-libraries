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

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(DAEMONIZER, 0x0302)

DEFINE_ERROR_CODE(DAEMONIZER_INVALID_INPUT, DAEMONIZER, 0x0001,
                  "The provided input is invalid.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(DAEMONIZER_FAILED_PARSING_INPUT, DAEMONIZER, 0x0002,
                  "The provided input could not be parsed.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(DAEMONIZER_FAILED_WAITING_FOR_LAUNCHED_PROCESSES, DAEMONIZER,
                  0x0003, "Failed while waiting for launched processes.",
                  HttpStatusCode::REQUEST_TIMEOUT)

DEFINE_ERROR_CODE(DAEMONIZER_UNKNOWN_ERROR, DAEMONIZER, 0x0004,
                  "The main processing loop exited with an unknown reason.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
