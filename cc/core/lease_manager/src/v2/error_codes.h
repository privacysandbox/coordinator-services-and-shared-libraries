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

/// Registers component code as 0x0067 for Lease Manager V2
REGISTER_COMPONENT_CODE(SC_LEASE_MANAGER_V2, 0x0067)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_NOT_INITIALIZED, SC_LEASE_MANAGER_V2, 0x0001,
                  "The lease manager service is not initialized.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_ALREADY_RUNNING, SC_LEASE_MANAGER_V2, 0x0002,
                  "The lease manager service is already running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_NOT_RUNNING, SC_LEASE_MANAGER_V2, 0x0003,
                  "The lease manager service is not running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_MANAGER_LOCK_CANNOT_BE_ADDED_WHILE_RUNNING,
                  SC_LEASE_MANAGER_V2, 0x0004,
                  "The lock cannot be added while lease manager is running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_LEASE_LIVENESS_ENFORCER_INSUFFICIENT_PERIOD, SC_LEASE_MANAGER_V2, 0x0006,
    "Lease liveness enforcer's enforcement period is insufficient.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_LIVENESS_ENFORCER_NOT_RUNNING, SC_LEASE_MANAGER_V2,
                  0x0007, "Lease liveness enforcer is not running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_REFRESHER_NOT_RUNNING, SC_LEASE_MANAGER_V2, 0x0008,
                  "Lease refresher is not running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_REFRESHER_ALREADY_RUNNING, SC_LEASE_MANAGER_V2,
                  0x0009, "Lease refresher is not running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_LIVENESS_ENFORCER_LIVENESS_VIOLATION,
                  SC_LEASE_MANAGER_V2, 0x000A,
                  "Lease liveness enforcer detected a violation in liveness",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_LEASE_MANAGER_RELEASE_NOTIFICATION_CANNOT_BE_PROCESSED,
    SC_LEASE_MANAGER_V2, 0x000B,
    "Lease Manager safe to release notification cannot be processed",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_LIVENESS_ENFORCER_ALREADY_RUNNING,
                  SC_LEASE_MANAGER_V2, 0x000C, "Already running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_REFRESHER_INVALID_STATE_WHEN_SETTING_REFRESH_MODE,
                  SC_LEASE_MANAGER_V2, 0x000D,
                  "Invalid state when setting lease refresh mode",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_LEASE_LIVENESS_ENFORCER_CANNOT_SET_THREAD_PRIORITY,
                  SC_LEASE_MANAGER_V2, 0x000E,
                  "Cannot set priority of the lease enforcer thread",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
