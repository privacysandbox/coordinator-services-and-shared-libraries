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
REGISTER_COMPONENT_CODE(SC_ROMA_FUNCTION_INVOKER_SAPI_IPC, 0x0BC0)
DEFINE_ERROR_CODE(
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_INVOKE_WITH_UNINITIALIZED_COMMS,
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC, 0x0001,
    "A call to invoke was made with an uninitialized comms object.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_COULD_NOT_SEND_CALL_TO_PARENT,
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC, 0x0002,
    "Could not send the call to the parent process.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_COULD_NOT_RECV_RESPONSE_FROM_PARENT,
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC, 0x0003,
    "Could not receive a response from the parent process.",
    HttpStatusCode::BAD_REQUEST)

REGISTER_COMPONENT_CODE(SC_ROMA_FUNCTION_TABLE, 0x0CC0)

DEFINE_ERROR_CODE(SC_ROMA_FUNCTION_TABLE_COULD_NOT_FIND_FUNCTION_NAME,
                  SC_ROMA_FUNCTION_TABLE, 0x0001,
                  "Could not find the function by name in the table.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_FUNCTION_TABLE_NAME_ALREADY_REGISTERED, SC_ROMA_FUNCTION_TABLE,
    0x0003,
    "A function with this name has already been registered in the table.",
    HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
