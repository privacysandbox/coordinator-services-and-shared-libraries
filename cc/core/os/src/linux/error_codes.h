// Copyright 2023 Google LLC
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
REGISTER_COMPONENT_CODE(SYSTEM_RESOURCE_INFO_PROVIDER_LINUX, 0x0F00)
DEFINE_ERROR_CODE(
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_OPEN_MEMINFO_FILE,
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX, 0x0001,
    "Could not open the meminfo file to check memory usage.",
    HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_PARSE_MEMINFO_LINE,
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX, 0x0002,
    "The line read from the meminfo file was not in the expected format.",
    HttpStatusCode::PRECONDITION_FAILED)

DEFINE_ERROR_CODE(
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_FIND_MEMORY_INFO,
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX, 0x0003,
    "The meminfo file did not contain the expected items.",
    HttpStatusCode::PRECONDITION_FAILED)
}  // namespace google::scp::core::errors
