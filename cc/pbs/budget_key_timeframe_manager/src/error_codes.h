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

REGISTER_COMPONENT_CODE(SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0104)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_TIMEFRAME_MANAGER_ENTRY_IS_LOADING,
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0001,
                  "The entry is being loaded.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG,
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0002,
                  "The entry metadata is invalid.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_TIMEFRAME_MANAGER_ENTRY_DOES_NOT_MATCH_LOG,
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0003,
                  "The entry metadata does not match.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_TIMEFRAME_MANAGER_CANNOT_BE_UNLOADED,
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0004,
                  "The budget key timeframe manager cannot be unloaded.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA,
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0005,
                  "The budget key timeframe data is corrupted.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_TIMEFRAME_MANAGER_EMPTY_REQUEST,
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0006,
                  "The budget key timeframe manager request is empty.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_BUDGET_KEY_TIMEFRAME_MANAGER_MULTIPLE_TIMEFRAME_GROUPS,
    SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0007,
    "The budget key timeframe manager request has more than one time group.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_BUDGET_KEY_TIMEFRAME_MANAGER_REPEATED_TIMEBUCKETS,
    SC_BUDGET_KEY_TIMEFRAME_MANAGER, 0x0008,
    "The budget key timeframe manager request does not have unique time "
    "buckets",
    HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
