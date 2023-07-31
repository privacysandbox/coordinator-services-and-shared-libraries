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

/// Registers component code as 0x000C for sized or timed bytes buffer.
REGISTER_COMPONENT_CODE(SC_SIZED_OR_TIMED_BYTES_BUFFER, 0x000C)

DEFINE_ERROR_CODE(SC_SIZED_OR_TIMED_BYTES_BUFFER_BUFFER_IS_SEALED,
                  SC_SIZED_OR_TIMED_BYTES_BUFFER, 0x0001,
                  "The buffer is already sealed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_ENOUGH_SPACE,
                  SC_SIZED_OR_TIMED_BYTES_BUFFER, 0x0002,
                  "The buffer does not have more space to store data.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_SIZED_OR_TIMED_BYTES_BUFFER_NOT_INITIALIZED,
                  SC_SIZED_OR_TIMED_BYTES_BUFFER, 0x0003,
                  "The buffer is not initialized.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
