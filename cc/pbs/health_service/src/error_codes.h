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

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(SC_PBS_HEALTH_SERVICE, 0x0910)
DEFINE_ERROR_CODE(
    SC_PBS_HEALTH_SERVICE_COULD_NOT_OPEN_MEMINFO_FILE, SC_PBS_HEALTH_SERVICE,
    0x0001,
    "The health service could not open the meminfo file to check memory usage.",
    HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(
    SC_PBS_HEALTH_SERVICE_COULD_NOT_PARSE_MEMINFO_LINE, SC_PBS_HEALTH_SERVICE,
    0x0002,
    "The line read from the meminfo file was not in the expected format.",
    HttpStatusCode::PRECONDITION_FAILED)

DEFINE_ERROR_CODE(SC_PBS_HEALTH_SERVICE_COULD_NOT_FIND_MEMORY_INFO,
                  SC_PBS_HEALTH_SERVICE, 0x0003,
                  "The meminfo file did not contain the expected items.",
                  HttpStatusCode::PRECONDITION_FAILED)

DEFINE_ERROR_CODE(SC_PBS_HEALTH_SERVICE_HEALTHY_MEMORY_USAGE_THRESHOLD_EXCEEDED,
                  SC_PBS_HEALTH_SERVICE, 0x0004,
                  "The healthy memory usage threshold has been exceeded.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_PBS_HEALTH_SERVICE_COULD_NOT_READ_FILESYSTEM_INFO, SC_PBS_HEALTH_SERVICE,
    0x0005, "The health service could not read the filesystem information.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_HEALTH_SERVICE_INVALID_READ_FILESYSTEM_INFO,
                  SC_PBS_HEALTH_SERVICE, 0x0006,
                  "The health service read invalid filesystem info.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_PBS_HEALTH_SERVICE_HEALTHY_STORAGE_USAGE_THRESHOLD_EXCEEDED,
    SC_PBS_HEALTH_SERVICE, 0x0007,
    "The healthy storage usage threshold has been exceeded.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
