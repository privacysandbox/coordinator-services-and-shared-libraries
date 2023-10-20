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
/// Registers component code as 0x0224 for gcp queue client provider.
REGISTER_COMPONENT_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0224)

DEFINE_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED,
    SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0001,
    "GCP Queue client failed to init due to invalid client options",
    HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED,
                  SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0002,
                  "GCP Queue client failed to init due to invalid queue name",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_CONFIG_VISIBILITY_TIMEOUT,
    SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0003,
    "GCP Queue client failed to init due to invalid visibility timeout",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED,
                  SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0004,
                  "GCP Queue client failed to init due to missing publisher",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED,
                  SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0005,
                  "GCP Queue client failed to init due to missing subscriber",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE, SC_GCP_QUEUE_CLIENT_PROVIDER,
    0x0006, "Cannot execute PubSub operation due to invalid message context",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT,
    SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0007,
    "Cannot execute PubSub operation due to invalid visibility timeout",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_MISMATCH,
                  SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0008,
                  "Cannot execute PubSub operation due to number of messages "
                  "from the response is not the same as in request",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED,
    SC_GCP_QUEUE_CLIENT_PROVIDER, 0x0009,
    "The number of messages receiving from SQS exceed maximum number",
    HttpStatusCode::INTERNAL_SERVER_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED,
    SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_CONFIG_VISIBILITY_TIMEOUT,
    SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE,
                         SC_CPIO_CLOUD_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT,
    SC_CPIO_CLOUD_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_MISMATCH,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED,
                         SC_CPIO_INTERNAL_ERROR)
}  // namespace google::scp::core::errors
