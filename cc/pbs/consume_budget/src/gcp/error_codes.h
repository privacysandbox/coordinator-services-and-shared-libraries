// Copyright 2024 Google LLC
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

#ifndef CC_PBS_CONSUME_BUDGET_SRC_GCP_ERROR_CODES_H_
#define CC_PBS_CONSUME_BUDGET_SRC_GCP_ERROR_CODES_H_

#include "cc/core/interface/errors.h"

namespace google::scp::pbs::errors {
REGISTER_COMPONENT_CODE(SC_PBS_CONSUME_BUDGET, 0x0157)

DEFINE_ERROR_CODE(
    SC_CONSUME_BUDGET_INITIALIZATION_ERROR, SC_PBS_CONSUME_BUDGET, 0x0001,
    "Failed to initialize BudgetConsumptionHelper.",
    privacy_sandbox::pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_CONSUME_BUDGET_PARSING_ERROR, SC_PBS_CONSUME_BUDGET, 0x0002,
    "Failed parse value in database.",
    privacy_sandbox::pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_CONSUME_BUDGET_FAIL_TO_COMMIT, SC_PBS_CONSUME_BUDGET, 0x0003,
    "Failed commit to database.",
    privacy_sandbox::pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_CONSUME_BUDGET_EXHAUSTED, SC_PBS_CONSUME_BUDGET, 0x0004,
                  "Failed to consume budget because budget is exhausted.",
                  privacy_sandbox::pbs_common::HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(
    SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA,
    SC_PBS_CONSUME_BUDGET, 0x0005,
    "The budget key timeframe data is corrupted.",
    privacy_sandbox::pbs_common::HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::pbs::errors

#endif  // CC_PBS_CONSUME_BUDGET_SRC_GCP_ERROR_CODES_H_
