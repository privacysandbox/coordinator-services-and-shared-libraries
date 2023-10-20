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

/// Registers component code as 0x0221 for AwsPrivateKeyFetcherProvider.
REGISTER_COMPONENT_CODE(SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER, 0x0221)

DEFINE_ERROR_CODE(
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND,
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER, 0x0001,
    "No credentials provider found", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_REGION_NOT_FOUND,
                  SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER, 0x0002,
                  "No region found", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_SIGN,
                  SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER, 0x0003,
                  "Failed to sign HTTP request using V4 signer",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_GET_URI,
                  SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER, 0x0004,
                  "Failed to get URI from HTTP request",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND,
    SC_CPIO_COMPONENT_FAILED_INITIALIZED)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_REGION_NOT_FOUND,
                         SC_CPIO_COMPONENT_FAILED_INITIALIZED)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_GET_URI,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_SIGN,
                         SC_CPIO_INTERNAL_ERROR)
}  // namespace google::scp::core::errors
