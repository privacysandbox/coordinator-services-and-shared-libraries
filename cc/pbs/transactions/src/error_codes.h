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

/// Registers component code as 0x0101 for pbs transaction serializer.
REGISTER_COMPONENT_CODE(SC_PBS_TRANSACTION_COMMAND_SERIALIZER, 0x0101)

DEFINE_ERROR_CODE(SC_PBS_TRANSACTION_COMMAND_SERIALIZER_UNSUPPORTED,
                  SC_PBS_TRANSACTION_COMMAND_SERIALIZER, 0x0001,
                  "The command is not supported for serialization.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_VERSION,
                  SC_PBS_TRANSACTION_COMMAND_SERIALIZER, 0x0002,
                  "The command's version is invalid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_TRANSACTION_COMMAND_SERIALIZER_SERIALIZATION_FAILED,
                  SC_PBS_TRANSACTION_COMMAND_SERIALIZER, 0x0003,
                  "The command's serialization failed.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE,
                  SC_PBS_TRANSACTION_COMMAND_SERIALIZER, 0x0004,
                  "The command's type is invalid.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_TRANSACTION_LOG,
                  SC_PBS_TRANSACTION_COMMAND_SERIALIZER, 0x0005,
                  "The transaction log is invalid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED,
                  SC_PBS_TRANSACTION_COMMAND_SERIALIZER, 0x0006,
                  "The command's deserialization failed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
