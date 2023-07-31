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
REGISTER_COMPONENT_CODE(SC_ROMA_V8_WORKER, 0x0481)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_BAD_SOURCE_CODE, SC_ROMA_V8_WORKER, 0x0001,
                  "Failed due to bad source code string, invalid std::string.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE, SC_ROMA_V8_WORKER,
                  0x0002, "Failed to compile JavaScript code object.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE, SC_ROMA_V8_WORKER,
                  0x0003, "Failed to run JavaScript code object.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_BAD_HANDLER_NAME, SC_ROMA_V8_WORKER, 0x0004,
                  "Failed due to bad handler name, invalid std::string.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION, SC_ROMA_V8_WORKER,
                  0x0005, "Failed to get valid function handler.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS, SC_ROMA_V8_WORKER, 0x0006,
                  "Failed due to bad input arguments, invalid std::string.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE, SC_ROMA_V8_WORKER,
                  0x0007, "Failed to execute code object.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_RESULT_PARSE_FAILURE, SC_ROMA_V8_WORKER,
                  0x0008, "Failed to parse the execution result.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE, SC_ROMA_V8_WORKER,
                  0x000A, "Failed to compile wasm object.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE,
                  SC_ROMA_V8_WORKER, 0x000B, "Failed to create wasm object.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_SCRIPT_EXECUTION_TIMEOUT, SC_ROMA_V8_WORKER,
                  0x000C, "Code object execute timeout.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_WASM_OBJECT_RETRIEVAL_FAILURE,
                  SC_ROMA_V8_WORKER, 0x000D, "Failed to retrieval wasm object.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_BIND_UNBOUND_SCRIPT_FAILED,
                  SC_ROMA_V8_WORKER, 0x000E, "Failed to bind unbound script.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_UNKNOWN_CODE_TYPE, SC_ROMA_V8_WORKER,
                  0x000F, "Failed to unknown code type.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_FAILED_TO_PARSE_TIMEOUT_TAG,
                  SC_ROMA_V8_WORKER, 0x0010,
                  "Failed to parse timeout value from tags.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_UNMATCHED_CODE_VERSION_NUM,
                  SC_ROMA_V8_WORKER, 0x0011,
                  "Failed due to unmatched version number.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_UNKNOWN_WASM_RETURN_TYPE, SC_ROMA_V8_WORKER,
                  0x0012, "Failed due to unknown wasm return type.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_UNSET_ISOLATE_WITH_PRELOADED_CODE,
                  SC_ROMA_V8_WORKER, 0x0013,
                  "Failed due to no pre-loaded source code and valid isolate.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_WORKER_ITEM_WORKED_ON_BEFORE, SC_ROMA_V8_WORKER,
                  0x0014,
                  "The work item has already been worked on. This implies the "
                  "worker initially died while handling this item.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
