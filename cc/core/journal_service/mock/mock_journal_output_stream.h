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

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "core/interface/blob_storage_provider_interface.h"
#include "core/journal_service/src/journal_output_stream.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

namespace google::scp::core::journal_service::mock {
class MockJournalOutputStream : public core::JournalOutputStream {
 public:
  MockJournalOutputStream(
      const std::shared_ptr<std::string>& bucket_name,
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<BlobStorageClientInterface>&
          blob_storage_provider_client)
      : core::JournalOutputStream(
            bucket_name, partition_name, async_executor,
            blob_storage_provider_client,
            std::make_shared<cpio::MockAggregateMetric>()) {}

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&)>
      append_log_mock;

  std::function<ExecutionResult()> create_new_buffer_mock;
  std::function<size_t(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&)>
      get_serialized_log_byte_size_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&,
      BytesBuffer&, size_t&)>
      serialize_log_mock;

  std::function<ExecutionResult(BytesBuffer&, uint64_t,
                                std::function<void(ExecutionResult&)>)>
      write_journal_blob_mock;

  std::function<void(
      JournalId journal_id, std::function<void(ExecutionResult&)> callback,
      AsyncContext<PutBlobRequest, PutBlobResponse>& pub_blob_context)>
      on_write_journal_blob_callback_mock;

  std::function<void(const std::shared_ptr<std::list<core::AsyncContext<
                         journal_service::JournalStreamAppendLogRequest,
                         journal_service::JournalStreamAppendLogResponse>>>&,
                     JournalId)>
      write_back_mock;

  ExecutionResult AppendLog(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          journal_stream_append_log_context) noexcept override {
    if (append_log_mock) {
      return append_log_mock(journal_stream_append_log_context);
    }
    return JournalOutputStream::AppendLog(journal_stream_append_log_context);
  }

  ExecutionResult CreateNewBuffer() noexcept override {
    if (create_new_buffer_mock) {
      return create_new_buffer_mock();
    }
    return JournalOutputStream::CreateNewBuffer();
  }

  size_t GetSerializedLogByteSize(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          journal_stream_append_log_context) noexcept override {
    if (get_serialized_log_byte_size_mock) {
      return get_serialized_log_byte_size_mock(
          journal_stream_append_log_context);
    }
    return JournalOutputStream::GetSerializedLogByteSize(
        journal_stream_append_log_context);
  }

  ExecutionResult SerializeLog(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          journal_stream_append_log_context,
      BytesBuffer& buffer, size_t& bytes_serialized) noexcept override {
    if (serialize_log_mock) {
      return serialize_log_mock(journal_stream_append_log_context, buffer,
                                bytes_serialized);
    }
    return JournalOutputStream::SerializeLog(journal_stream_append_log_context,
                                             buffer, bytes_serialized);
  }

  ExecutionResult WriteJournalBlob(
      BytesBuffer& bytes_buffer, uint64_t current_journal_id,
      std::function<void(ExecutionResult&)> callback) noexcept override {
    if (write_journal_blob_mock) {
      return write_journal_blob_mock(bytes_buffer, current_journal_id,
                                     callback);
    }
    return JournalOutputStream::WriteJournalBlob(bytes_buffer,
                                                 current_journal_id, callback);
  }

  void OnWriteJournalBlobCallback(
      JournalId journal_id, std::function<void(ExecutionResult&)> callback,
      AsyncContext<PutBlobRequest, PutBlobResponse>& pub_blob_context) noexcept
      override {
    if (on_write_journal_blob_callback_mock) {
      on_write_journal_blob_callback_mock(journal_id, callback,
                                          pub_blob_context);
      return;
    }
    JournalOutputStream::OnWriteJournalBlobCallback(journal_id, callback,
                                                    pub_blob_context);
  }

  void WriteBatch(
      const std::shared_ptr<std::list<core::AsyncContext<
          journal_service::JournalStreamAppendLogRequest,
          journal_service::JournalStreamAppendLogResponse>>>& flush_batch,
      JournalId current_journal_id) noexcept {
    if (write_back_mock) {
      write_back_mock(flush_batch, current_journal_id);
      return;
    }

    JournalOutputStream::WriteBatch(flush_batch, current_journal_id);
  }

  auto& GetPersistedJournalIds() { return journals_to_persist_; }

  auto& GetPendingLogsCount() { return pending_logs_; }

  auto& GetPendingLogs() { return logs_queue_; }
};
}  // namespace google::scp::core::journal_service::mock
