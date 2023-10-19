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
/// Registers component code as 0x0110 for pbs transaction request router.
REGISTER_COMPONENT_CODE(SC_PBS_TRANSACTION_REQUEST_ROUTER, 0x0110)

DEFINE_ERROR_CODE(
    SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID,
    SC_PBS_TRANSACTION_REQUEST_ROUTER, 0x0001,
    "The request cannot be routed because of missing routing identifier.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE,
    SC_PBS_TRANSACTION_REQUEST_ROUTER, 0x0002,
    "The request cannot be routed because target partition is unavailable.",
    HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
