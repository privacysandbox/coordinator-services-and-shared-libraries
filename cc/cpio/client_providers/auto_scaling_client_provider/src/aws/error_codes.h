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
/// Registers component code as 0x0226 for AwsParameterClientProvider.
REGISTER_COMPONENT_CODE(SC_AWS_AUTO_SCALING_CLIENT_PROVIDER, 0x0228)

DEFINE_ERROR_CODE(SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_NOT_FOUND,
                  SC_AWS_AUTO_SCALING_CLIENT_PROVIDER, 0x0001,
                  "Instance not found", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_MULTIPLE_INSTANCES_FOUND,
                  SC_AWS_AUTO_SCALING_CLIENT_PROVIDER, 0x0002,
                  "Multiple instances are found for the same instance ID",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED,
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER, 0x0003, "Missing instance resource ID",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_LIFECYCLE_HOOK_NAME_REQUIRED,
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER, 0x0004, "Missing lifecycle hook name",
    HttpStatusCode::BAD_REQUEST)

MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_NOT_FOUND,
                         SC_CPIO_CLOUD_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_MULTIPLE_INSTANCES_FOUND,
    SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED,
    SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_LIFECYCLE_HOOK_NAME_REQUIRED,
    SC_CPIO_INVALID_REQUEST)
}  // namespace google::scp::core::errors
