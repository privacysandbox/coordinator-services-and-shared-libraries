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

/// Registers component code as 0x0108 for pbs service.
REGISTER_COMPONENT_CODE(SC_PBS_SERVICE, 0x0108)

DEFINE_ERROR_CODE(SC_PBS_SERVICE_ALREADY_RUNNING, SC_PBS_SERVICE, 0x0001,
                  "The PBS service is already running.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_SERVICE_NOT_RUNNING, SC_PBS_SERVICE, 0x0002,
                  "The PBS service is not currently running.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_PBS_SERVICE_RECOVERY_FAILED, SC_PBS_SERVICE, 0x0003,
                  "The PBS service recovery failed.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_PBS_LEASE_LOST, SC_PBS_SERVICE, 0x0004,
                  "The PBS service least has been lost.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH, SC_PBS_SERVICE, 0x0005,
    "The file path provided for the HTTP2 server private key is invalid.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH, SC_PBS_SERVICE, 0x0006,
    "The file path provided for the HTTP2 server certificate is invalid.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_SERVICE_UNRECOVERABLE_ERROR, SC_PBS_SERVICE, 0x0007,
                  "The PBS service encountered an unrecoverable error.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
