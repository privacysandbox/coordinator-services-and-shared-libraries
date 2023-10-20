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

/// Registers component code as 0x0213 for AWS metric client.
REGISTER_COMPONENT_CODE(SC_AWS_METRIC_CLIENT_PROVIDER, 0x0213)

DEFINE_ERROR_CODE(
    SC_AWS_METRIC_CLIENT_PROVIDER_METRIC_LIMIT_REACHED_PER_REQUEST,
    SC_AWS_METRIC_CLIENT_PROVIDER, 0x0001,
    "Exceeding the metric limit per request", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_REQUEST_PAYLOAD_OVERSIZE,
                  SC_AWS_METRIC_CLIENT_PROVIDER, 0x0002,
                  "Request payload must not have a size greater than 560000.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_TIMESTAMP,
    SC_AWS_METRIC_CLIENT_PROVIDER, 0x0003,
    "Timestamp cannot be two weeks before or 2 hours after the current "
    "date",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_VALUE,
                  SC_AWS_METRIC_CLIENT_PROVIDER, 0x0004,
                  "Invalid metric value cannot convert to double",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_AWS_METRIC_CLIENT_PROVIDER_OVERSIZE_DATUM_DIMENSIONS,
    SC_AWS_METRIC_CLIENT_PROVIDER, 0x0005,
    "AWS metric datum dimensions must not have a size greater than 30",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_UNIT,
                  SC_AWS_METRIC_CLIENT_PROVIDER, 0x0006, "Invalid metric unit",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_SHOULD_ENABLE_BATCH_RECORDING,
                  SC_AWS_METRIC_CLIENT_PROVIDER, 0x0007,
                  "Should enable batch recording",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_METRIC_CLIENT_PROVIDER_METRIC_LIMIT_REACHED_PER_REQUEST,
    SC_CPIO_REQUEST_TOO_LARGE)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_REQUEST_PAYLOAD_OVERSIZE,
                         SC_CPIO_REQUEST_TOO_LARGE)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_TIMESTAMP,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_VALUE,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_METRIC_CLIENT_PROVIDER_OVERSIZE_DATUM_DIMENSIONS,
    SC_CPIO_REQUEST_TOO_LARGE)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_UNIT,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_METRIC_CLIENT_PROVIDER_SHOULD_ENABLE_BATCH_RECORDING,
    SC_CPIO_INTERNAL_ERROR)
}  // namespace google::scp::core::errors
