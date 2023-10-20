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

/// Registers component code as 0x0105 for budget key transaction protocol.
REGISTER_COMPONENT_CODE(SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0105)

DEFINE_ERROR_CODE(SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS,
                  SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0001,
                  "An active write transaction is in progress.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID,
                  SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0002,
                  "Invalid transaction id.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_BUDGET_KEY_TRANSACTION_ID_MISMATCH,
                  SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0003,
                  "Transaction id mismatch.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET,
                  SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0004,
                  "Not enough budget to proceed.", HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_LESS_BUDGETS_TO_CONSUME,
                  SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0005,
                  "Batch consume budget request has < 2 budgets to consume.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_INVALID_ORDER,
                  SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0006,
                  "Batch consume budget request does not have time buckets in "
                  "increasing order.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_BUDGET_KEY_CONSUME_BUDGET_LOADED_TIMEFRAMES_INVALID,
                  SC_PBS_BUDGET_KEY_TRANSACTION_PROTOCOLS, 0x0007,
                  "Timeframes loaded are invalid.", HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
