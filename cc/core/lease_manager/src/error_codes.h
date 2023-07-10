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

/// Registers component code as 0x0016 for Lease Manager
REGISTER_COMPONENT_CODE(SC_LEASE_MANAGER, 0x0016)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_NOT_INITIALIZED, SC_LEASE_MANAGER, 0x0001,
                  "The lease manager service is not initialized.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_ALREADY_RUNNING, SC_LEASE_MANAGER, 0x0002,
                  "The lease manager service is already running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_NOT_RUNNING, SC_LEASE_MANAGER, 0x0003,
                  "The lease manager service is not running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_LOCK_CANNOT_BE_ADDED_WHILE_RUNNING,
                  SC_LEASE_MANAGER, 0x0004,
                  "The lock cannot be added while lease manager is running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASABLE_LOCK_TIMESTAMP_CONVERSION_ERROR, SC_LEASE_MANAGER,
                  0x0005,
                  "Error in converting between string and int64_t formats for "
                  "expiration timestamp value.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_LEASE_MANAGER_LOCK_LEASE_DURATION_INVALID, SC_LEASE_MANAGER, 0x0006,
    "Lease duration is invalid for the lease manager's configuration",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
