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

/// Registers component code as 0x0223 for PrivateKeyClientProvider.
REGISTER_COMPONENT_CODE(SC_PRIVATE_KEY_CLIENT_PROVIDER, 0x0223)

DEFINE_ERROR_CODE(SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND,
                  SC_PRIVATE_KEY_CLIENT_PROVIDER, 0x0001,
                  "Failed to find key data in private key fetching response.",
                  HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_PRIVATE_KEY_CLIENT_PROVIDER_SECRET_PIECE_SIZE_UNMATCHED,
                  SC_PRIVATE_KEY_CLIENT_PROVIDER, 0x0002,
                  "Failed due to unmatched secret piece size",
                  HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS,
    SC_PRIVATE_KEY_CLIENT_PROVIDER, 0x0003,
    "Failed due to unmatched endpoints number and key data splits",
    HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_RESOURCE_NAME,
                  SC_PRIVATE_KEY_CLIENT_PROVIDER, 0x0004,
                  "Key resource name is invalid",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_CLIENT_PROVIDER_SECRET_PIECE_SIZE_UNMATCHED,
    SC_CPIO_INVALID_RESOURCE)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS,
    SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_RESOURCE_NAME,
    SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
}  // namespace google::scp::core::errors
