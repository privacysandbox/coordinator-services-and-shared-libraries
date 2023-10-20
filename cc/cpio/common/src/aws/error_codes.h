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

/// Registers component code as 0x0212 for shared AWS errors.
REGISTER_COMPONENT_CODE(SC_AWS, 0x0212)

DEFINE_ERROR_CODE(SC_AWS_INTERNAL_SERVICE_ERROR, SC_AWS, 0x0009,
                  "Internal AWS server error",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_AWS_INVALID_CREDENTIALS, SC_AWS, 0x000A,
                  "Invalid AWS credentials", HttpStatusCode::UNAUTHORIZED)
DEFINE_ERROR_CODE(SC_AWS_INVALID_REQUEST, SC_AWS, 0x000B,
                  "Invalid AWS input request", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_REQUEST_LIMIT_REACHED, SC_AWS, 0x000C,
                  "Reach AWS request limit", HttpStatusCode::TOO_MANY_REQUESTS)
DEFINE_ERROR_CODE(SC_AWS_SERVICE_UNAVAILABLE, SC_AWS, 0x000D,
                  "AWS service unavailable",
                  HttpStatusCode::SERVICE_UNAVAILABLE)
DEFINE_ERROR_CODE(SC_AWS_VALIDATION_FAILED, SC_AWS, 0x000E,
                  "AWS input parameters validation failed",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_NOT_FOUND, SC_AWS, 0x000F, "AWS entity not found",
                  HttpStatusCode::NOT_FOUND)

MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_INTERNAL_SERVICE_ERROR,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_INVALID_CREDENTIALS,
                         SC_CPIO_CLOUD_INVALID_CREDENTIALS)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_INVALID_REQUEST, SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_REQUEST_LIMIT_REACHED,
                         SC_CPIO_CLOUD_REQUEST_LIMIT_REACHED)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_SERVICE_UNAVAILABLE,
                         SC_CPIO_CLOUD_SERVICE_UNAVAILABLE)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_VALIDATION_FAILED, SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_NOT_FOUND, SC_CPIO_CLOUD_NOT_FOUND)
}  // namespace google::scp::core::errors
