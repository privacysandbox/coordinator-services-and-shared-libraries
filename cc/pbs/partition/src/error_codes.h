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

/// Registers component code as 0x011A for the PBS partition.
REGISTER_COMPONENT_CODE(SC_PBS_PARTITION, 0x011A)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_NOT_LOADED, SC_PBS_PARTITION, 0x0001,
                  "PBS partition has not loaded yet",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_CANNOT_LOAD, SC_PBS_PARTITION, 0x0002,
                  "PBS partition cannot be loaded at this time",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_CANNOT_UNLOAD, SC_PBS_PARTITION, 0x0003,
                  "PBS partition cannot be unloaded at this time",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_INVALID_PARTITON_STATE, SC_PBS_PARTITION,
                  0x0004, "PBS partition state is invalid",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_RECOVERY_FAILED, SC_PBS_PARTITION, 0x0005,
                  "PBS partition recovery failed.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_INVALID_TRANSACTION, SC_PBS_PARTITION,
                  0x0006, "PBS partition received an invalid transaction.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_CANNOT_INITIALIZE, SC_PBS_PARTITION, 0x0007,
                  "PBS partition cannot be initialized at this time.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_IS_REMOTE_CANNOT_HANDLE_REQUEST,
                  SC_PBS_PARTITION, 0x0008,
                  "This is a remote partition and request cannot be handled.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
