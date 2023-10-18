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
REGISTER_COMPONENT_CODE(SC_ROMA_V8_ENGINE, 0x0A00)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_ERROR_COMPILING_SCRIPT, SC_ROMA_V8_ENGINE,
                  0x0001, "An error was found while compiling the script.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_ERROR_RUNNING_SCRIPT, SC_ROMA_V8_ENGINE,
                  0x0002, "An error was found while running the script.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_COULD_NOT_FIND_HANDLER_BY_NAME,
                  SC_ROMA_V8_ENGINE, 0x0003,
                  "Could not find a handler by the given name in the code.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER, SC_ROMA_V8_ENGINE,
                  0x0004, "Error when invoking the handler.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_COULD_NOT_CREATE_ISOLATE, SC_ROMA_V8_ENGINE,
                  0x0005, "Creating the isolate failed.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_JSON,
                  SC_ROMA_V8_ENGINE, 0x0006, "Error converting output to JSON.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING,
                  SC_ROMA_V8_ENGINE, 0x0007,
                  "Error converting output to native string.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT,
                  SC_ROMA_V8_ENGINE, 0x0008,
                  "Error parsing input as valid JSON.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_COULD_NOT_REGISTER_FUNCTION_BINDING,
                  SC_ROMA_V8_ENGINE, 0x0009,
                  "Error while registering function binding in v8 context.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_V8_ENGINE_ISOLATE_ALREADY_INITIALIZED, SC_ROMA_V8_ENGINE, 0x000A,
    "The v8 isolate has already been initialized before call to Init.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_ISOLATE_NOT_INITIALIZED, SC_ROMA_V8_ENGINE,
                  0x000B,
                  "The v8 isolate has not been initialized. The module has not "
                  "been initialized.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_V8_ENGINE_CREATE_COMPILATION_CONTEXT_FAILED_WITH_EMPTY_CODE,
    SC_ROMA_V8_ENGINE, 0x000C,
    "Create compilation context failed with empty source code.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT, SC_ROMA_V8_ENGINE,
                  0x000D, "V8 execution terminated due to timeout.",
                  HttpStatusCode::BAD_REQUEST)

REGISTER_COMPONENT_CODE(SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING, 0x0A01)

DEFINE_ERROR_CODE(SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_INVALID_ISOLATE,
                  SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING, 0x0001,
                  "The v8 isolate passed to the visitor is invalid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_EMPTY_CONTEXT,
                  SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING, 0x0002,
                  "The v8 context in the isolate is empty.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
