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

/// Registers component code as 0x000B for serialization.
REGISTER_COMPONENT_CODE(SC_SERIALIZATION, 0x000B)

DEFINE_ERROR_CODE(SC_SERIALIZATION_BUFFER_NOT_WRITABLE, SC_SERIALIZATION,
                  0x0001, "Cannot write to the current buffer.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_SERIALIZATION_BUFFER_NOT_READABLE, SC_SERIALIZATION,
                  0x0002, "Cannot read from the current buffer.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_SERIALIZATION_PROTO_SERIALIZATION_FAILED, SC_SERIALIZATION,
                  0x0003, "Proto serialization failed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED,
                  SC_SERIALIZATION, 0x0004, "Proto deserialization failed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_SERIALIZATION_INVALID_SERIALIZATION_TYPE, SC_SERIALIZATION,
                  0x0005, "Invalid serialization type.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_SERIALIZATION_VERSION_IS_INVALID, SC_SERIALIZATION, 0x0006,
                  "The proto message is invalid.", HttpStatusCode::BAD_REQUEST)

}  // namespace google::scp::core::errors
