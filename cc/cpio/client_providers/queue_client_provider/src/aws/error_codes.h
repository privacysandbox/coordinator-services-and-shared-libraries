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
REGISTER_COMPONENT_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0227)

DEFINE_ERROR_CODE(
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED,
    SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0001,
    "AWS Queue client failed to init due to invalid client options",
    HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED,
                  SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0002,
                  "AWS Queue client failed to init due to invalid queue name",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE,
                  SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0003,
                  "Cannot execute SQS operation due to invalid message context",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED,
    SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0004,
    "The number of messages receiving from SQS exceed maximum number",
    HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO,
                  SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0005,
                  "Cannot execute SQS operation due to invalid receipt info",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT,
    SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0006,
    "Cannot execute SQS operation due to invalid visibility timeout",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT,
                  SC_AWS_QUEUE_CLIENT_PROVIDER, 0x0007,
                  "Cannot execute SQS operation due to the message assoicated "
                  "with the receipt info is not in flight",
                  HttpStatusCode::BAD_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED,
    SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED,
                         SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO,
                         SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT,
    SC_CPIO_INVALID_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT,
                         SC_CPIO_INVALID_REQUEST)
}  // namespace google::scp::core::errors
