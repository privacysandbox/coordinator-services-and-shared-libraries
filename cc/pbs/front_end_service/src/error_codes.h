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

#include "cc/core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

/// Registers component code as 0x0100 for pbs frontend service.
REGISTER_COMPONENT_CODE(SC_PBS_FRONT_END_SERVICE, 0x0100)

/// Defines the error code as 0x0001 when the request type is already
/// subscribed.
DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST,
                  SC_PBS_FRONT_END_SERVICE, 0x0001, "The request is invalid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY,
                  SC_PBS_FRONT_END_SERVICE, 0x0002,
                  "The request body is invalid.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_NO_KEYS_AVAILABLE,
                  SC_PBS_FRONT_END_SERVICE, 0x0003,
                  "The request body does not contain keys.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY,
                  SC_PBS_FRONT_END_SERVICE, 0x0004,
                  "The response body is invalid.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND,
                  SC_PBS_FRONT_END_SERVICE, 0x0005,
                  "The request header not found.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_BAD_TRANSACTON_COMMANDS,
                  SC_PBS_FRONT_END_SERVICE, 0x0006,
                  "Bad commands while processing transaction commands.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_BEGIN_TRANSACTION_DISALLOWED,
                  SC_PBS_FRONT_END_SERVICE, 0x0007,
                  "Front end does not allow new transactions at this time.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
