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

/// Registers component code as 0x0145 for pbs partition lease preference
/// applier.
REGISTER_COMPONENT_CODE(SC_PBS_PARTITION_LEASE_PREF_APPLIER, 0x0145)

DEFINE_ERROR_CODE(
    SC_PBS_PARTITION_LEASE_PREF_APPLIER_ALREADY_RUNNING,
    SC_PBS_PARTITION_LEASE_PREF_APPLIER, 0x0001,
    "The PBS Partition Lease Preference Applier is already running.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_LEASE_PREF_APPLIER_NOT_RUNNING,
                  SC_PBS_PARTITION_LEASE_PREF_APPLIER, 0x0002,
                  "The PBS Partition Lease Preference Applier is not running.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_LEASE_PREF_APPLIER_NO_OP,
                  SC_PBS_PARTITION_LEASE_PREF_APPLIER, 0x0003,
                  "The PBS Partition Lease Preference Applier cannot perform "
                  "the application.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
