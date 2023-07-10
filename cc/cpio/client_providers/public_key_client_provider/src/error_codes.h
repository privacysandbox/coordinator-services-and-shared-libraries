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
/// Registers component code as 0x0219 for public key client provider.
REGISTER_COMPONENT_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER, 0x0219)

DEFINE_ERROR_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED,
                  SC_PUBLIC_KEY_CLIENT_PROVIDER, 0x0001,
                  "Public key client failed to fetch the public keys",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED,
                  SC_PUBLIC_KEY_CLIENT_PROVIDER, 0x0002,
                  "Public key client failed to fetch the expired time",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER_INVALID_CONFIG_OPTIONS,
                  SC_PUBLIC_KEY_CLIENT_PROVIDER, 0x0003,
                  "Public key client config options endpoints cannot be empty",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER_HTTP_CLIENT_REQUIRED,
                  SC_PUBLIC_KEY_CLIENT_PROVIDER, 0x0004,
                  "Public key client failed to init due to invalid http client",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(
    SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED,
    SC_PUBLIC_KEY_CLIENT_PROVIDER, 0x0005,
    "Public key client failed to perform request for config endpoints",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED,
    SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER_INVALID_CONFIG_OPTIONS,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_PUBLIC_KEY_CLIENT_PROVIDER_HTTP_CLIENT_REQUIRED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED,
    SC_CPIO_INVALID_REQUEST)
}  // namespace google::scp::core::errors
