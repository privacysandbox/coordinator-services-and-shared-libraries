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
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/interface/blob_storage_provider_interface.h"
#include "core/journal_service/src/journal_input_stream.h"

namespace google::scp::core::journal_service::mock {
class MockJournalInputStream : public core::JournalInputStream {
 public:
  MockJournalInputStream(
      std::shared_ptr<std::string>& bucket_name,
      std::shared_ptr<std::string>& partition_name,
      std::shared_ptr<BlobStorageClientInterface>& blob_storage_provider_client,
      std::shared_ptr<ConfigProviderInterface> config_provider)
      : core::JournalInputStream(bucket_name, partition_name,
                                 blob_storage_provider_client,
                                 config_provider) {}

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&)>
      read_log_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&)>
      read_last_checkpoint_blob_mock;

  std::function<void(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      AsyncContext<GetBlobRequest, GetBlobResponse>&)>
      on_read_last_checkpoint_blob_callback_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      size_t)>
      read_checkpoint_blob_mock;

  std::function<void(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      AsyncContext<GetBlobRequest, GetBlobResponse>&)>
      on_read_checkpoint_blob_callback_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      size_t, size_t)>
      read_journal_blob_mock;

  std::function<void(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      AsyncContext<GetBlobRequest, GetBlobResponse>&, size_t)>
      on_read_journal_blob_callback_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      std::vector<JournalId>&)>
      read_journal_blobs_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      std::shared_ptr<Blob>&)>
      list_checkpoints_mock;

  std::function<void(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&)>
      on_list_checkpoints_callback_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      std::shared_ptr<Blob>&)>
      list_journals_mock;

  std::function<void(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&,
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&)>
      on_list_journals_callback_mock;

  std::function<ExecutionResult(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&)>
      process_loaded_journals_mock;

  std::function<ExecutionResult(core::Timestamp&, JournalLogStatus&,
                                core::common::Uuid&, core::common::Uuid&,
                                journal_service::JournalLog&, JournalId&)>
      process_next_journal_log_mock;

  ExecutionResult ReadLastCheckpointBlob(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context) noexcept override {
    if (read_last_checkpoint_blob_mock) {
      return read_last_checkpoint_blob_mock(read_journal_input_stream_context);
    }
    return JournalInputStream::ReadLastCheckpointBlob(
        read_journal_input_stream_context);
  }

  ExecutionResult ReadLog(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context) noexcept override {
    if (read_log_mock) {
      return read_log_mock(read_journal_input_stream_context);
    }

    return JournalInputStream::ReadLog(read_journal_input_stream_context);
  }

  void OnReadLastCheckpointBlobCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept
      override {
    if (on_read_last_checkpoint_blob_callback_mock) {
      on_read_last_checkpoint_blob_callback_mock(
          read_journal_input_stream_context, get_blob_context);
      return;
    }
    JournalInputStream::OnReadLastCheckpointBlobCallback(
        read_journal_input_stream_context, get_blob_context);
  }

  ExecutionResult ReadCheckpointBlob(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      JournalId checkpoint_index) noexcept override {
    if (read_checkpoint_blob_mock) {
      return read_checkpoint_blob_mock(read_journal_input_stream_context,
                                       checkpoint_index);
    }
    return JournalInputStream::ReadCheckpointBlob(
        read_journal_input_stream_context, checkpoint_index);
  }

  void OnReadCheckpointBlobCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept
      override {
    if (on_read_checkpoint_blob_callback_mock) {
      on_read_checkpoint_blob_callback_mock(read_journal_input_stream_context,
                                            get_blob_context);
      return;
    }
    JournalInputStream::OnReadCheckpointBlobCallback(
        read_journal_input_stream_context, get_blob_context);
  }

  ExecutionResult ReadJournalBlob(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      JournalId journal_index, size_t buffer_index) noexcept override {
    if (read_journal_blob_mock) {
      return read_journal_blob_mock(read_journal_input_stream_context,
                                    journal_index, buffer_index);
    }
    return JournalInputStream::ReadJournalBlob(
        read_journal_input_stream_context, journal_index, buffer_index);
  }

  void OnReadJournalBlobCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context,
      size_t buffer_index) noexcept override {
    if (on_read_journal_blob_callback_mock) {
      on_read_journal_blob_callback_mock(read_journal_input_stream_context,
                                         get_blob_context, buffer_index);
      return;
    }
    JournalInputStream::OnReadJournalBlobCallback(
        read_journal_input_stream_context, get_blob_context, buffer_index);
  }

  ExecutionResult ReadJournalBlobs(
      AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
          journal_stream_read_log_context,
      std::vector<JournalId>& journal_ids) noexcept override {
    if (read_journal_blobs_mock) {
      return read_journal_blobs_mock(journal_stream_read_log_context,
                                     journal_ids);
    }
    return JournalInputStream::ReadJournalBlobs(journal_stream_read_log_context,
                                                journal_ids);
  }

  ExecutionResult ListCheckpoints(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      std::shared_ptr<Blob>& start_from) noexcept override {
    if (list_checkpoints_mock) {
      return list_checkpoints_mock(read_journal_input_stream_context,
                                   start_from);
    }
    return JournalInputStream::ListCheckpoints(
        read_journal_input_stream_context, start_from);
  }

  void OnListCheckpointsCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&
          list_blobs_context) noexcept override {
    if (on_list_checkpoints_callback_mock) {
      on_list_checkpoints_callback_mock(read_journal_input_stream_context,
                                        list_blobs_context);
      return;
    }
    JournalInputStream::OnListCheckpointsCallback(
        read_journal_input_stream_context, list_blobs_context);
  }

  ExecutionResult ListJournals(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      std::shared_ptr<Blob>& start_from) noexcept override {
    if (list_journals_mock) {
      return list_journals_mock(read_journal_input_stream_context, start_from);
    }
    return JournalInputStream::ListJournals(read_journal_input_stream_context,
                                            start_from);
  }

  void OnListJournalsCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&
          list_blobs_context) noexcept override {
    if (on_list_journals_callback_mock) {
      on_list_journals_callback_mock(read_journal_input_stream_context,
                                     list_blobs_context);
      return;
    }
    JournalInputStream::OnListJournalsCallback(
        read_journal_input_stream_context, list_blobs_context);
  }

  ExecutionResult ProcessLoadedJournals(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context) noexcept override {
    if (process_loaded_journals_mock) {
      return process_loaded_journals_mock(read_journal_input_stream_context);
    }
    return JournalInputStream::ProcessLoadedJournals(
        read_journal_input_stream_context);
  }

  ExecutionResult ProcessNextJournalLog(
      core::Timestamp& timestamp, JournalLogStatus& journal_log_status,
      core::common::Uuid& component_id, core::common::Uuid& log_id,
      journal_service::JournalLog& log,
      JournalId& journal_id) noexcept override {
    if (process_next_journal_log_mock) {
      return process_next_journal_log_mock(
          timestamp, journal_log_status, component_id, log_id, log, journal_id);
    }
    return JournalInputStream::ProcessNextJournalLog(
        timestamp, journal_log_status, component_id, log_id, log, journal_id);
  }

  ExecutionResult ReadJournalLogBatch(
      std::shared_ptr<std::vector<JournalStreamReadLogObject>>&
          journal_batch) noexcept override {
    return JournalInputStream::ReadJournalLogBatch(journal_batch);
  }

  void SetLastProcessedJournalId(JournalId journal_id) {
    last_processed_journal_id_ = journal_id;
  }

  const JournalId& GetLastCheckpointId() { return last_checkpoint_id_; }

  std::atomic<size_t>& GetTotalJournalsToRead() {
    return total_journals_to_read_;
  }

  std::vector<BytesBuffer>& GetJournalBuffers() { return journal_buffers_; }

  std::vector<JournalId>& GetJournalIds() { return journal_ids_; }

  bool& GetJournalsLoaded() { return journals_loaded_; }

  size_t& GetCurrentBufferIndex() { return current_buffer_index_; }

  size_t& GetCurrentBufferOffset() { return current_buffer_offset_; }

  void set_journal_ids_loaded(bool journal_ids_loaded) {
    journal_ids_loaded_ = journal_ids_loaded;
  }
};
}  // namespace google::scp::core::journal_service::mock
