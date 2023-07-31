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

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

/// Registers component code as 0x0109 for async executor.
REGISTER_COMPONENT_CODE(SC_PBS_CLIENT, 0x0109)

DEFINE_ERROR_CODE(SC_PBS_CLIENT_RESPONSE_HEADER_NOT_FOUND, SC_PBS_CLIENT,
                  0x0001, "Transaction response headers are missing.",
                  HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_PBS_CLIENT_INVALID_RESPONSE_HEADER, SC_PBS_CLIENT, 0x0002,
                  "Transaction response headers are invalid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_CLIENT_INVALID_PHASE, SC_PBS_CLIENT, 0x0003,
                  "The transaction phase is invalid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_CLIENT_NO_BUDGET_KEY_PROVIDED, SC_PBS_CLIENT, 0x0004,
                  "No budget key provided.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_CLIENT_INVALID_TRANSACTION_METADATA, SC_PBS_CLIENT,
                  0x0005, "Invalid transaction metadata.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
