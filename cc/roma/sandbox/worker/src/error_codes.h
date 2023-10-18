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
REGISTER_COMPONENT_CODE(SC_ROMA_WORKER, 0x0900)
DEFINE_ERROR_CODE(SC_ROMA_WORKER_MISSING_METADATA_ITEM, SC_ROMA_WORKER, 0x0001,
                  "Missing expected key in request metadata.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_MISSING_CONTEXT_WHEN_EXECUTING, SC_ROMA_WORKER,
                  0x0002,
                  "Could not find a stored context for the execution request.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_REQUEST_TYPE_NOT_SUPPORTED, SC_ROMA_WORKER,
                  0x0003, "The request type is not yet supported.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_STR_CONVERT_INT_FAIL, SC_ROMA_WORKER, 0x0004,
                  "Cannot convert string to integer.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
