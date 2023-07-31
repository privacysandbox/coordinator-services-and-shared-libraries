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
/// Registers component code as 0x0206 for crypto client provider.
REGISTER_COMPONENT_CODE(SC_CRYPTO_CLIENT_PROVIDER, 0x0206)

DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0001, "Cannot create AEAD",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_AEAD_ENCRYPT_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0002,
                  "Cannot encrypt payload using AEAD",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_AEAD_DECRYPT_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0003,
                  "Cannot decrypt payload using AEAD",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0004,
                  "Cannot create HPKE context", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_HPKE_ENCRYPT_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0005,
                  "Cannot encrypt payload using HPKE",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_HPKE_DECRYPT_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0006,
                  "Cannot decrypt payload using HPKE",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0007, "Cannot export secret",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_SPLIT_CIPHERTEXT_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x0008, "Cannot split ciphertext",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_READ_BINARY_KEY_SET_FROM_PRIVATE_KEY,
    SC_CRYPTO_CLIENT_PROVIDER, 0x0009,
    "Cannot read binary key set from private key", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x000A,
                  "Cannot create keyset handle", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_INVALID_KEYSET_SIZE,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x000B, "Invalid keyset size",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PRIVATE_KEY_FAILED,
                  SC_CRYPTO_CLIENT_PROVIDER, 0x000C,
                  "Failed to parse Hpke private key",
                  HttpStatusCode::BAD_REQUEST)

MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_AEAD_ENCRYPT_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_AEAD_DECRYPT_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_HPKE_ENCRYPT_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_HPKE_DECRYPT_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_SPLIT_CIPHERTEXT_FAILED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_READ_BINARY_KEY_SET_FROM_PRIVATE_KEY,
    SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_INVALID_KEYSET_SIZE,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PRIVATE_KEY_FAILED,
    SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE,
                         SC_CPIO_INVALID_REQUEST)
}  // namespace google::scp::core::errors
