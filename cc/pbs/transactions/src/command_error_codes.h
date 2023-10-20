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

/// Registers component code as 0x0156 for pbs transaction command.
REGISTER_COMPONENT_CODE(SC_PBS_TRANSACTION_COMMAND, 0x0156)

DEFINE_ERROR_CODE(
    SC_PBS_TRANSACTION_COMMAND_DEPENDENCIES_UNINITIALIZED,
    SC_PBS_TRANSACTION_COMMAND, 0x0001,
    "The command's execution dependencies are not initialized yet.",
    HttpStatusCode::BAD_REQUEST)

}  // namespace google::scp::core::errors
