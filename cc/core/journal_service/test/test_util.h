// Copyright 2023 Google LLC
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

#include <string>
#include <vector>

#include "cc/core/journal_service/src/proto/journal_service.pb.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/interface/type_def.h"
#include "core/journal_service/interface/journal_service_stream_interface.h"
#include "core/journal_service/src/journal_input_stream.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::journal_service::test_util {

ExecutionResult WriteFile(
    const BytesBuffer& bytes_buffer, std::string_view file_name,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client);

ExecutionResultOr<BytesBuffer> JournalLogToBytesBuffer(
    const journal_service::JournalLog& journal_log);

ExecutionResult WriteJournalLog(
    const journal_service::JournalLog& journal_log,
    std::string_view journal_file_postfix,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client);

ExecutionResult WriteJournalLogs(
    const std::vector<journal_service::JournalLog>& journal_logs,
    std::string_view journal_file_postfix,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client);

ExecutionResult WriteCheckpoint(
    const journal_service::JournalLog& journal_log,
    JournalId last_processed_journal_id, std::string_view file_postfix,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client);

ExecutionResult WriteLastCheckpoint(
    CheckpointId checkpoint_id,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client);

std::string JournalIdToString(uint64_t journal_id);
AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
ReadLogs(const JournalStreamReadLogRequest& request,
         JournalInputStream& journal_input_stream);
}  // namespace google::scp::core::journal_service::test_util
