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

namespace privacy_sandbox::pbs {

/// Registers component code as 0x0100 for pbs frontend service.
REGISTER_COMPONENT_CODE(SC_PBS_FRONT_END_SERVICE, 0x0100)

/// Defines the error code as 0x0001 when the request type is already
/// subscribed.
DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST,
                  SC_PBS_FRONT_END_SERVICE, 0x0001, "The request is invalid.",
                  pbs_common::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY,
                  SC_PBS_FRONT_END_SERVICE, 0x0002,
                  "The request body is invalid.",
                  pbs_common::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_NO_KEYS_AVAILABLE,
                  SC_PBS_FRONT_END_SERVICE, 0x0003,
                  "The request body does not contain keys.",
                  pbs_common::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY,
                  SC_PBS_FRONT_END_SERVICE, 0x0004,
                  "The response body is invalid.",
                  pbs_common::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND,
                  SC_PBS_FRONT_END_SERVICE, 0x0005,
                  "The request header not found.",
                  pbs_common::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_BAD_TRANSACTON_COMMANDS,
                  SC_PBS_FRONT_END_SERVICE, 0x0006,
                  "Bad commands while processing transaction commands.",
                  pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_BEGIN_TRANSACTION_DISALLOWED,
                  SC_PBS_FRONT_END_SERVICE, 0x0007,
                  "Front end does not allow new transactions at this time.",
                  pbs_common::HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INVALID_REPORTING_ORIGIN,
                  SC_PBS_FRONT_END_SERVICE, 0x0008,
                  "The request contains an invalid reporting_origin.",
                  pbs_common::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE,
                  SC_PBS_FRONT_END_SERVICE, 0x0009,
                  "The request contains a reporting_origin that does not "
                  "belong to the provided site.",
                  pbs_common::HttpStatusCode::UNAUTHORIZED)

DEFINE_ERROR_CODE(SC_PBS_FRONT_END_SERVICE_INITIALIZATION_FAILED,
                  SC_PBS_FRONT_END_SERVICE, 0x0011,
                  "Failed to initialize FrontEndService.",
                  pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_PBS_FRONT_END_SERVICE_GET_TRANSACTION_STATUS_RETURNS_404_BY_DEFAULT,
    SC_PBS_FRONT_END_SERVICE, 0x0012,
    "Return 404 by default when GetTransactionStatus is called.",
    pbs_common::HttpStatusCode::NOT_FOUND)

}  // namespace privacy_sandbox::pbs
