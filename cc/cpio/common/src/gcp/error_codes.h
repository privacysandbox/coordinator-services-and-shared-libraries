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
#include "public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {

/// Registers component code as 0x0230 for shared GCP errors.
REGISTER_COMPONENT_CODE(SC_GCP, 0x0230)

DEFINE_ERROR_CODE(SC_GCP_INTERNAL_SERVICE_ERROR, SC_GCP, 0x0001,
                  "Internal GCP server error",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_GCP_NOT_FOUND, SC_GCP, 0x0002, "GCP not found",
                  HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_GCP_INVALID_ARGUMENT, SC_GCP, 0x0003,
                  "GCP invalid argument", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_GCP_DEADLINE_EXCEEDED, SC_GCP, 0x0004,
                  "GCP deadline exceeded", HttpStatusCode::REQUEST_TIMEOUT)
DEFINE_ERROR_CODE(SC_GCP_UNAUTHENTICATED, SC_GCP, 0x0005, "GCP unauthenticated",
                  HttpStatusCode::UNAUTHORIZED)
DEFINE_ERROR_CODE(SC_GCP_CANCELLED, SC_GCP, 0x0006, "GCP cancelled",
                  HttpStatusCode::CANCELLED)
DEFINE_ERROR_CODE(SC_GCP_ALREADY_EXISTS, SC_GCP, 0x0007, "GCP already exists",
                  HttpStatusCode::CONFLICT)
DEFINE_ERROR_CODE(SC_GCP_OUT_OF_RANGE, SC_GCP, 0x0008, "GCP out of range",
                  HttpStatusCode::REQUEST_RANGE_NOT_SATISFIABLE)
DEFINE_ERROR_CODE(SC_GCP_UNIMPLEMENTED, SC_GCP, 0x0009, "GCP unimplemented",
                  HttpStatusCode::NOT_IMPLEMENTED)
DEFINE_ERROR_CODE(SC_GCP_UNAVAILABLE, SC_GCP, 0x000A, "GCP unavailable",
                  HttpStatusCode::SERVICE_UNAVAILABLE)
DEFINE_ERROR_CODE(SC_GCP_UNKNOWN, SC_GCP, 0x000B, "GCP unknown",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_GCP_PERMISSION_DENIED, SC_GCP, 0x000C,
                  "GCP permission denied", HttpStatusCode::UNAUTHORIZED)
DEFINE_ERROR_CODE(SC_GCP_RESOURCE_EXHAUSTED, SC_GCP, 0x000D,
                  "GCP resource exhausted", HttpStatusCode::NO_CONTENT)
DEFINE_ERROR_CODE(SC_GCP_FAILED_PRECONDITION, SC_GCP, 0x000E,
                  "GCP failed precondition", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_GCP_ABORTED, SC_GCP, 0x000F, "GCP aborted",
                  HttpStatusCode::CANCELLED)
DEFINE_ERROR_CODE(SC_GCP_DATA_LOSS, SC_GCP, 0x0010, "GCP data loss",
                  HttpStatusCode::NO_CONTENT)

MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_INTERNAL_SERVICE_ERROR,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_NOT_FOUND, SC_CPIO_CLOUD_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_INVALID_ARGUMENT,
                         SC_CPIO_CLOUD_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_DEADLINE_EXCEEDED,
                         SC_CPIO_CLOUD_REQUEST_TIMEOUT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_UNAUTHENTICATED,
                         SC_CPIO_CLOUD_INVALID_CREDENTIALS)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_CANCELLED, SC_CPIO_CLOUD_REQUEST_ABORTED)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_ALREADY_EXISTS, SC_CPIO_CLOUD_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_OUT_OF_RANGE, SC_CPIO_CLOUD_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_UNIMPLEMENTED, SC_CPIO_CLOUD_NOT_IMPLEMENTED)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_UNAVAILABLE, SC_CPIO_CLOUD_SERVICE_UNAVAILABLE)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_UNKNOWN, SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_PERMISSION_DENIED,
                         SC_CPIO_CLOUD_INVALID_CREDENTIALS)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_RESOURCE_EXHAUSTED,
                         SC_CPIO_CLOUD_SERVICE_UNAVAILABLE)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_FAILED_PRECONDITION,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_ABORTED, SC_CPIO_CLOUD_REQUEST_ABORTED)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_DATA_LOSS, SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)

}  // namespace google::scp::core::errors
