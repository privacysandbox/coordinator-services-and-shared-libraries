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

#ifndef SCP_CPIO_INTERFACE_ERROR_CODES_H_
#define SCP_CPIO_INTERFACE_ERROR_CODES_H_

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
/// Registers component code as 0x0214 for CPIO common errors.
REGISTER_COMPONENT_CODE(SC_CPIO, 0x0214)

DEFINE_ERROR_CODE(SC_CPIO_INTERNAL_ERROR, SC_CPIO, 0x0001,
                  "Internal Error in CPIO",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR, SC_CPIO, 0x0002,
                  "Internal Error in Cloud Service",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_INVALID_CREDENTIALS, SC_CPIO, 0x0003,
                  "Invalid Cloud credentials", HttpStatusCode::UNAUTHORIZED)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_REQUEST_LIMIT_REACHED, SC_CPIO, 0x0004,
                  "Reach request limit in Cloud Service",
                  HttpStatusCode::TOO_MANY_REQUESTS)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_SERVICE_UNAVAILABLE, SC_CPIO, 0x0005,
                  "Cloud service unavailable",
                  HttpStatusCode::SERVICE_UNAVAILABLE)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_NOT_IMPLEMENTED, SC_CPIO, 0x000F,
                  "Not implemented", HttpStatusCode::NOT_IMPLEMENTED)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_REQUEST_TIMEOUT, SC_CPIO, 0x0010,
                  "Request timeout", HttpStatusCode::REQUEST_TIMEOUT)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_NOT_FOUND, SC_CPIO, 0x0011, "Not found",
                  HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_INVALID_ARGUMENT, SC_CPIO, 0x0012,
                  "Invalid argument", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_REQUEST_ABORTED, SC_CPIO, 0x0013,
                  "Request aborted", HttpStatusCode::CANCELLED)
DEFINE_ERROR_CODE(SC_CPIO_CLOUD_ALREADY_EXISTS, SC_CPIO, 0x0014,
                  "Already exists", HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_CPIO_COMPONENT_FAILED_INITIALIZED, SC_CPIO, 0x0006,
                  "The component is failed to initialized",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_CPIO_COMPONENT_NOT_RUNNING, SC_CPIO, 0x0007,
                  "The component is not running in CPIO",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_CPIO_COMPONENT_ALREADY_RUNNING, SC_CPIO, 0x0008,
                  "The component is already running in CPIO",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_CPIO_INVALID_REQUEST, SC_CPIO, 0x0009, "Invalid Request",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CPIO_REQUEST_TOO_LARGE, SC_CPIO, 0x000A,
                  "Parameters in request exceeded limit",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CPIO_INVALID_RESOURCE, SC_CPIO, 0x000B,
                  "Resources validation failed", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CPIO_RESOURCE_NOT_FOUND, SC_CPIO, 0x000C,
                  "Resources not found", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_CPIO_ENTITY_NOT_FOUND, SC_CPIO, 0x000D, "Entity not found",
                  HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_CPIO_MULTIPLE_ENTITIES_FOUND, SC_CPIO, 0x000E,
                  "Multiple Entities found",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors

#endif  // SCP_CPIO_INTERFACE_ERROR_CODES_H_
