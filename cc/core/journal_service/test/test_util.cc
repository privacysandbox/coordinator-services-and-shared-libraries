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
#include "core/journal_service/test/test_util.h"

#include <gmock/gmock-matchers.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "cc/core/journal_service/src/journal_utils.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/journal_service/src/journal_serialization.h"

namespace google::scp::core::journal_service::test_util {

using ::google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using ::google::scp::core::journal_service::CheckpointMetadata;
using ::google::scp::core::journal_service::JournalLog;
using ::google::scp::core::journal_service::JournalSerialization;
using ::google::scp::core::journal_service::LastCheckpointMetadata;

constexpr char kBucketName[] = "fake_bucket";
constexpr char kPartitionName[] = "fake_partition";
constexpr char kLastCheckpointBlobName[] = "last_checkpoint";

ExecutionResult WriteFile(
    const BytesBuffer& bytes_buffer, std::string_view file_name,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client) {
  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
  put_blob_context.request = std::make_shared<PutBlobRequest>();
  put_blob_context.request->bucket_name =
      std::make_shared<std::string>(kBucketName);
  put_blob_context.request->blob_name = std::make_shared<std::string>(
      absl::StrCat(kPartitionName, "/", file_name));
  put_blob_context.request->buffer =
      std::make_shared<BytesBuffer>(bytes_buffer);
  absl::Notification put_blob_notification;

  AsyncContext<PutBlobRequest, PutBlobResponse> callback_context;
  put_blob_context.callback =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse> context) {
        callback_context = context;
        put_blob_notification.Notify();
      };
  if (auto result = mock_storage_client.PutBlob(put_blob_context);
      !result.Successful()) {
    return result;
  }
  put_blob_notification.WaitForNotification();
  return callback_context.result;
}

ExecutionResultOr<BytesBuffer> JournalLogToBytesBuffer(
    const JournalLog& journal_log) {
  size_t journal_log_byte_size = 0;
  if (auto result = JournalSerialization::CalculateSerializationByteSize(
          journal_log, journal_log_byte_size);
      !result.Successful()) {
    return result;
  }

  size_t byte_size_required = kLogHeaderByteLength + journal_log_byte_size;
  BytesBuffer journal_bytes_buffer(byte_size_required);
  journal_bytes_buffer.length = byte_size_required;
  size_t bytes_serialized = 0;
  if (auto result = JournalSerialization::SerializeLogHeader(
          journal_bytes_buffer, 0, Timestamp(), JournalLogStatus::Log,
          /*component_uuid*/ {0x1, 0x2}, /*log_uuid=*/{0x3, 0x4},
          bytes_serialized);
      !result.Successful()) {
    return result;
  }

  if (auto result = JournalSerialization::SerializeJournalLog(
          journal_bytes_buffer, bytes_serialized, journal_log,
          bytes_serialized);
      !result.Successful()) {
    return result;
  }
  return std::move(journal_bytes_buffer);
}

ExecutionResult WriteJournalLog(
    const JournalLog& journal_log, std::string_view journal_file_postfix,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client) {
  return WriteJournalLogs({journal_log}, journal_file_postfix,
                          mock_storage_client);
}

ExecutionResult WriteJournalLogs(
    const std::vector<JournalLog>& journal_logs,
    std::string_view journal_file_postfix,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client) {
  std::vector<std::string> journal_logs_serialized(journal_logs.size());
  for (const JournalLog& journal_log : journal_logs) {
    auto journal_bytes_buffer = JournalLogToBytesBuffer(journal_log);
    if (!journal_bytes_buffer.Successful()) {
      return journal_bytes_buffer.result();
    }
    journal_logs_serialized.push_back(journal_bytes_buffer->ToString());
  }

  BytesBuffer journal_blob(absl::StrJoin(journal_logs_serialized, ""));
  return WriteFile(journal_blob,
                   absl::StrCat(kJournalBlobNamePrefix, journal_file_postfix),
                   mock_storage_client);
}

ExecutionResult WriteCheckpoint(
    const JournalLog& journal_log, JournalId last_processed_journal_id,
    std::string_view file_postfix,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client) {
  auto journal_bytes_buffer = JournalLogToBytesBuffer(journal_log);
  if (!journal_bytes_buffer.Successful()) {
    return journal_bytes_buffer.result();
  }

  CheckpointMetadata checkpoint_metdata;
  checkpoint_metdata.set_last_processed_journal_id(last_processed_journal_id);
  size_t byte_size_required = 0;
  if (auto result = JournalSerialization::CalculateSerializationByteSize(
          checkpoint_metdata, byte_size_required);
      !result.Successful()) {
    return result;
  }

  BytesBuffer checkpoint_bytes_buffer;
  checkpoint_bytes_buffer.bytes =
      std::make_shared<std::vector<Byte>>(byte_size_required);
  checkpoint_bytes_buffer.capacity = byte_size_required;
  checkpoint_bytes_buffer.length = byte_size_required;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;

  if (auto result = JournalSerialization::SerializeCheckpointMetadata(
          checkpoint_bytes_buffer, buffer_offset, checkpoint_metdata,
          bytes_serialized);
      !result.Successful()) {
    return result;
  }
  BytesBuffer bytes_buffer(absl::StrCat(journal_bytes_buffer->ToString(),
                                        checkpoint_bytes_buffer.ToString()));
  return WriteFile(bytes_buffer,
                   absl::StrCat(kCheckpointBlobNamePrefix, file_postfix),
                   mock_storage_client);
}

ExecutionResult WriteLastCheckpoint(
    CheckpointId checkpoint_id,
    blob_storage_provider::mock::MockBlobStorageClient& mock_storage_client) {
  LastCheckpointMetadata last_checkpoint_metadata;
  last_checkpoint_metadata.set_last_checkpoint_id(checkpoint_id);

  BytesBuffer last_checkpoint_buffer(1000);
  size_t current_bytes_serialized = 0;
  if (auto result = JournalSerialization::SerializeLastCheckpointMetadata(
          last_checkpoint_buffer, 0, last_checkpoint_metadata,
          current_bytes_serialized);
      !result.Successful()) {
    return result;
  }
  last_checkpoint_buffer.length = current_bytes_serialized;
  return WriteFile(last_checkpoint_buffer, kLastCheckpointBlobName,
                   mock_storage_client);
}

std::string JournalIdToString(uint64_t journal_id) {
  auto suffix = std::to_string(journal_id);
  auto suffix_with_zero_prefix =
      std::string(kZerosInSuffix - std::min(kZerosInSuffix, suffix.length()),
                  '0') +
      suffix;
  return suffix_with_zero_prefix;
}

AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
ReadLogs(const JournalStreamReadLogRequest& request,
         JournalInputStream& journal_input_stream) {
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context;
  context.request = std::make_shared<JournalStreamReadLogRequest>(request);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      callback_context;

  absl::Notification notification;
  context.callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                      JournalStreamReadLogResponse>&
                             journal_stream_read_log_context) {
    callback_context = journal_stream_read_log_context;
    notification.Notify();
  };
  EXPECT_TRUE(journal_input_stream.ReadLog(context).Successful());
  notification.WaitForNotification();
  return callback_context;
}
}  // namespace google::scp::core::journal_service::test_util
