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

/// Registers component code as 0x0210 for AwsInstanceClientProvider.
REGISTER_COMPONENT_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER, 0x0210)

DEFINE_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_RESOURCE_NAME,
                  SC_AWS_INSTANCE_CLIENT_PROVIDER, 0x0001,
                  "Invalid resource name", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_INSTANCE_ID,
                  SC_AWS_INSTANCE_CLIENT_PROVIDER, 0x0003,
                  "Invalid instance ID", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_MULTIPLE_TAG_VALUES_FOUND,
                  SC_AWS_INSTANCE_CLIENT_PROVIDER, 0x0004,
                  "Multiple tag values found for one tag",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_RESOURCE_NOT_FOUND,
                  SC_AWS_INSTANCE_CLIENT_PROVIDER, 0x0005,
                  "The resource not found", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_NOT_ALL_TAG_VALUES_FOUND,
                  SC_AWS_INSTANCE_CLIENT_PROVIDER, 0x0006,
                  "The tag value not found", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_TAG_NAME,
                  SC_AWS_INSTANCE_CLIENT_PROVIDER, 0x0007, "Invalid tag name",
                  HttpStatusCode::BAD_REQUEST)

MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_RESOURCE_NAME,
                         SC_CPIO_INVALID_RESOURCE)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_INSTANCE_ID,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_INSTANCE_CLIENT_PROVIDER_MULTIPLE_TAG_VALUES_FOUND,
    SC_CPIO_MULTIPLE_ENTITIES_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_RESOURCE_NOT_FOUND,
                         SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_INSTANCE_CLIENT_PROVIDER_NOT_ALL_TAG_VALUES_FOUND,
    SC_CPIO_ENTITY_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_TAG_NAME,
                         SC_CPIO_INVALID_REQUEST)

}  // namespace google::scp::core::errors
