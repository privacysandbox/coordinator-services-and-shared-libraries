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

/// Registers component code as 0x010F for the PBS Partition Manager.
REGISTER_COMPONENT_CODE(SC_PBS_PARTITION_MANAGER, 0x010F)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_MANAGER_ALREADY_RUNNING,
                  SC_PBS_PARTITION_MANAGER, 0x0001,
                  "PBS partition manager already running",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_MANAGER_NOT_RUNNING,
                  SC_PBS_PARTITION_MANAGER, 0x0002,
                  "PBS partition manager not currently running",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_MANAGER_INVALID_REQUEST,
                  SC_PBS_PARTITION_MANAGER, 0x0003,
                  "PBS partition manager got an invalid request",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_UNLOAD_FAILURE, SC_PBS_PARTITION_MANAGER,
                  0x0004, "PBS partition manager cannot unload a partition",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_PBS_PARTITION_LOAD_FAILURE, SC_PBS_PARTITION_MANAGER,
                  0x0005, "PBS partition manager cannot load a partition",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
