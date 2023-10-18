/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "cc/core/interface/errors.h"
#include "public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {
/// Registers component code as 0x001A for blob storage provider.
REGISTER_COMPONENT_CODE(SC_BLOB_STORAGE_PROVIDER, 0x0207)

DEFINE_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND,
                  SC_BLOB_STORAGE_PROVIDER, 0x0001, "Blob path not found.",
                  HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB,
                  SC_BLOB_STORAGE_PROVIDER, 0x0002,
                  "Error occurred while getting the blob.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR,
                  SC_BLOB_STORAGE_PROVIDER, 0x0003,
                  "Invalid Blob Storage request failed and cannot be retried.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR, SC_BLOB_STORAGE_PROVIDER, 0x0004,
    "Invalid Blob Storage Database request failed and can be retried.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS,
                  SC_BLOB_STORAGE_PROVIDER, 0x0005,
                  "Invalid arguments provided.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED,
                  SC_BLOB_STORAGE_PROVIDER, 0x0006, "Stream session expired.",
                  HttpStatusCode::REQUEST_TIMEOUT)

DEFINE_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED,
                  SC_BLOB_STORAGE_PROVIDER, 0x0007, "Stream session cancelled.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_EMPTY_ETAG, SC_BLOB_STORAGE_PROVIDER,
                  0x0008, "ETag is empty.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND,
                         SC_CPIO_ENTITY_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS,
                         SC_CPIO_CLOUD_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED,
                         SC_CPIO_CLOUD_REQUEST_TIMEOUT)
MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_BLOB_STORAGE_PROVIDER_EMPTY_ETAG,
                         SC_CPIO_CLOUD_INTERNAL_SERVICE_ERROR)

}  // namespace google::scp::core::errors
