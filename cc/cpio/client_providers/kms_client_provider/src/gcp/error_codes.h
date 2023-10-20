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
#include "public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {
/// Registers component code as 0x0225 for GCP KMS client provider.
REGISTER_COMPONENT_CODE(SC_GCP_KMS_CLIENT_PROVIDER, 0x0225)

DEFINE_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND,
                  SC_GCP_KMS_CLIENT_PROVIDER, 0x0001, "Cannot find the key arn",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND,
                  SC_GCP_KMS_CLIENT_PROVIDER, 0x0002, "Cannot find cipher text",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED,
                  SC_GCP_KMS_CLIENT_PROVIDER, 0x0003, "Cannot create AEAD",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED,
                  SC_GCP_KMS_CLIENT_PROVIDER, 0x0004, "Cannot decrpyt",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED,
                  SC_GCP_KMS_CLIENT_PROVIDER, 0x0005,
                  "Cannot decode the data using base64",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED,
                         SC_CPIO_INTERNAL_ERROR)

}  // namespace google::scp::core::errors
