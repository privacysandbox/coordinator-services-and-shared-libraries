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
/// Registers component code as 0x0231 for job client provider.
REGISTER_COMPONENT_CODE(SC_JOB_CLIENT_PROVIDER, 0x0231)

DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_JOB_CLIENT_OPTIONS_REQUIRED,
                  SC_JOB_CLIENT_PROVIDER, 0x0001,
                  "Job client failed to init due to invalid job options",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_SERIALIZATION_FAILED,
                  SC_JOB_CLIENT_PROVIDER, 0x0002,
                  "Job client failed to create job due to serialzation failed",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_DESERIALIZATION_FAILED,
                  SC_JOB_CLIENT_PROVIDER, 0x0003,
                  "Job client failed to get job due to deserialization failed",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(
    SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM, SC_JOB_CLIENT_PROVIDER, 0x0004,
    "Job client failed to get job from database due to invalid job item",
    HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID, SC_JOB_CLIENT_PROVIDER,
                  0x0005,
                  "Job client failed to operate request due to missing job id",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS, SC_JOB_CLIENT_PROVIDER, 0x0006,
    "Job client failed to operate request due to invalid job status",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO, SC_JOB_CLIENT_PROVIDER, 0x0007,
    "Job client failed to operate request due to invalid receipt info",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_INVALID_DURATION,
                  SC_JOB_CLIENT_PROVIDER, 0x0008,
                  "Job client failed to update job visibility timeout due to "
                  "invalid duration",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT,
                  SC_JOB_CLIENT_PROVIDER, 0x0009,
                  "Job client failed to update job due to resource "
                  "modification conflicts with another request",
                  HttpStatusCode::CONFLICT)
DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED,
                  SC_JOB_CLIENT_PROVIDER, 0x000A,
                  "Job client failed to create job due to job entry creation "
                  "failed in database",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION,
                  SC_JOB_CLIENT_PROVIDER, 0x000B,
                  "Job client failed to create job due to job is already "
                  "created in database with another server job id",
                  HttpStatusCode::BAD_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_JOB_CLIENT_OPTIONS_REQUIRED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_SERIALIZATION_FAILED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_DESERIALIZATION_FAILED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_INVALID_DURATION,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT,
                         SC_CPIO_CLOUD_ALREADY_EXISTS)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION,
                         SC_CPIO_INVALID_REQUEST)
}  // namespace google::scp::core::errors
