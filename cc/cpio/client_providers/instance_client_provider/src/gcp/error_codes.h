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

/// Registers component code as 0x0220 for GcpInstanceClientProvider.
REGISTER_COMPONENT_CODE(SC_GCP_INSTANCE_CLIENT_PROVIDER, 0x0220)

DEFINE_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_PROVIDER_SERVICE_UNAVAILABLE,
                  SC_GCP_INSTANCE_CLIENT_PROVIDER, 0x0001,
                  "Gcp Instance Provider is unavailable due to remote error",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE,
                  SC_GCP_INSTANCE_CLIENT_PROVIDER, 0x0002,
                  "Gcp Instance Provider failed to parse instance zone value",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED,
                  SC_GCP_INSTANCE_CLIENT_PROVIDER, 0x0003,
                  "Get instance details http response is malformed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME,
                  SC_GCP_INSTANCE_CLIENT_PROVIDER, 0x0004,
                  "Invalid instance resource ID", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED,
                  SC_GCP_INSTANCE_CLIENT_PROVIDER, 0x0005,
                  "Get resource tags http response is malformed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_TYPE,
                  SC_GCP_INSTANCE_CLIENT_PROVIDER, 0x0006,
                  "Invalid instance resource type.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_PROVIDER_SERVICE_UNAVAILABLE,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED,
    SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED,
    SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_TYPE,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
}  // namespace google::scp::core::errors
