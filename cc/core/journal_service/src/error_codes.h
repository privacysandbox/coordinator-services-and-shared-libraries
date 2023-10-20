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
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
/// Registers component code as 0x000D for journal service.
REGISTER_COMPONENT_CODE(SC_JOURNAL_SERVICE, 0x000D)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_INVALID_BLOB_NAME, SC_JOURNAL_SERVICE,
                  0x0001, "The provided blob name is not valid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_CANNOT_SERIALIZE, SC_JOURNAL_SERVICE,
                  0x0002, "Data cannot be serialized.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_CORRUPTED_BLOB, SC_JOURNAL_SERVICE, 0x0003,
                  "The blob is corrupted.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_MAGIC_NUMBER_NOT_MATCHING,
                  SC_JOURNAL_SERVICE, 0x0004,
                  "The checkpoint magic number not matching",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN,
                  SC_JOURNAL_SERVICE, 0x0005,
                  "No more logs to return in the input stream.",
                  HttpStatusCode::NOT_MODIFIED)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME,
                  SC_JOURNAL_SERVICE, 0x0006, "Blob name cannot be created.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_ALREADY_INITIALIZED, SC_JOURNAL_SERVICE,
                  0x0007, "The journal service is already initialized.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_NOT_INITIALIZED, SC_JOURNAL_SERVICE,
                  0x0008, "The journal service is not initialized.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_ALREADY_RUNNING, SC_JOURNAL_SERVICE,
                  0x0009, "The journal service is already running.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_ALREADY_STOPPED, SC_JOURNAL_SERVICE,
                  0x000A, "The journal service is already stopped.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_CANNOT_SUBSCRIBE_WHEN_RUNNING,
                  SC_JOURNAL_SERVICE, 0x000B,
                  "Cannot subscribe to the journal service when it is running.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_JOURNAL_SERVICE_CANNOT_UNSUBSCRIBE_WHEN_RUNNING, SC_JOURNAL_SERVICE,
    0x000C, "Cannot unsubscribe from the journal service when it is running.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_NO_NEW_JOURNAL_ID_AVAILABLE,
                  SC_JOURNAL_SERVICE, 0x000D,
                  "No new journal has been written.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_JOURNAL_SERVICE_OUTPUT_STREAM_JOURNAL_STATE_NOT_FOUND,
    SC_JOURNAL_SERVICE, 0x000E,
    "Journal state is not found in callback after journal is written.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_CORRUPTED_BATCH_OF_LOGS,
                  SC_JOURNAL_SERVICE, 0x000F,
                  "The batch of logs to flush is corrupted.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LAST_CHECKPOINT,
                  SC_JOURNAL_SERVICE, 0x0010,
                  "The last checkpoint metadata blob is invalid.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LISTING,
                  SC_JOURNAL_SERVICE, 0x0012,
                  "The blob/checkpoint listing is invalid.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_JOURNAL_SERVICE_NO_OUTPUT_STREAM, SC_JOURNAL_SERVICE, 0x0013,
    "No JournalOutputStream is available in JournalServer. This could happen "
    "when logs recovery process is not yet completed",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOURNAL_SERVICE_FAILED_TO_FLUSH_BATCH_OF_LOGS,
                  SC_JOURNAL_SERVICE, 0x0014,
                  "The batch of logs to flush failed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
