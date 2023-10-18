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

/// Registers component code as 0x0222 for PrivateKeyFetcherProvider.
REGISTER_COMPONENT_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0222)

DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0001,
                  "No http client found", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_PUBLIC_KEYSET_HANDLE_NOT_FOUND,
    SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0002,
    "Cannot find the public keyset handle", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_RESOURCE_NAME_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0003,
                  "Cannot find the resource name", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_ENCRYPTION_KEY_TYPE_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0004,
                  "Cannot find encryption key type", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_EXPIRATION_TIME_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0005,
                  "Cannot find the expiration time", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_DATA_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0006,
                  "Cannot find the key data", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_JSON_TAG_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0007,
                  "Cannot find the json tag", HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0008,
                  "Failed with invalid encryption key type",
                  HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_MATERIAL_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x0009,
                  "Cannot find the public key signature",
                  HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_PUBLIC_KEY_MATERIAL_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x000A,
                  "Cannot find the public key material",
                  HttpStatusCode::NOT_FOUND)
DEFINE_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_CREATION_TIME_NOT_FOUND,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x000B,
                  "Cannot find the expiration time", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_RESOURCE_NAME,
                  SC_PRIVATE_KEY_FETCHER_PROVIDER, 0x000C,
                  "The resource name for encryption key is invalid",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND,
                         SC_CPIO_COMPONENT_FAILED_INITIALIZED)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_RESOURCE_NAME_NOT_FOUND,
    SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_ENCRYPTION_KEY_TYPE_NOT_FOUND,
    SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_EXPIRATION_TIME_NOT_FOUND,
    SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_CREATION_TIME_NOT_FOUND,
    SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_DATA_NOT_FOUND,
                         SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_PUBLIC_KEYSET_HANDLE_NOT_FOUND,
    SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_JSON_TAG_NOT_FOUND,
                         SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE,
    SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_MATERIAL_NOT_FOUND,
                         SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PRIVATE_KEY_FETCHER_PROVIDER_PUBLIC_KEY_MATERIAL_NOT_FOUND,
    SC_CPIO_RESOURCE_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_RESOURCE_NAME,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
}  // namespace google::scp::core::errors
