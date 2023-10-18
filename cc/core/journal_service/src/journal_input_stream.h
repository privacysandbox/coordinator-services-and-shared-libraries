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
#include <memory>
#include <string>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/journal_service_interface.h"
#include "core/journal_service/interface/journal_service_stream_interface.h"
#include "core/journal_service/src/proto/journal_service.pb.h"

namespace google::scp::core {

static constexpr size_t kDefaultNumberOfJournalsToReadPerBatch = 1000;
static constexpr size_t kDefaultNumberOfJournalLogsToReturn = 5000;

/*! @copydoc JournalInputStreamInterface
 */
class JournalInputStream : public journal_service::JournalInputStreamInterface {
 public:
  JournalInputStream(
      std::shared_ptr<std::string> bucket_name,
      std::shared_ptr<std::string> partition_name,
      std::shared_ptr<BlobStorageClientInterface> blob_storage_provider_client,
      std::shared_ptr<ConfigProviderInterface> config_provider)
      : journals_loaded_(false),
        last_checkpoint_id_(kInvalidCheckpointId),
        last_processed_journal_id_(kInvalidJournalId),
        total_journals_to_read_(0),
        execution_result_of_failed_journal_read_(SuccessExecutionResult()),
        is_any_journal_read_failed_(false),
        current_buffer_index_(0),
        current_buffer_offset_(0),
        bucket_name_(bucket_name),
        partition_name_(partition_name),
        blob_storage_provider_client_(blob_storage_provider_client),
        config_provider_(config_provider),
        journal_ids_window_start_index_(0),
        journal_ids_window_length_(0),
        enable_batch_read_journals_(false),
        number_of_journals_per_batch_(kDefaultNumberOfJournalsToReadPerBatch),
        number_of_journal_logs_to_return_(kDefaultNumberOfJournalLogsToReturn) {
    if (!config_provider_->Get(kPBSJournalInputStreamEnableBatchReadJournals,
                               enable_batch_read_journals_)) {
      enable_batch_read_journals_ = false;
    }
    if (!config_provider_->Get(kPBSJournalInputStreamNumberOfJournalsPerBatch,
                               number_of_journals_per_batch_)) {
      number_of_journals_per_batch_ = kDefaultNumberOfJournalsToReadPerBatch;
    }
    if (!config_provider_->Get(
            kPBSJournalInputStreamNumberOfJournalLogsToReturn,
            number_of_journal_logs_to_return_)) {
      number_of_journal_logs_to_return_ = kDefaultNumberOfJournalLogsToReturn;
    }
  }

  /**
   * @brief ReadLog() returns a set of logs read from the stream. The set may
   * return End of Stream by returning a status code
   * SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN, after which
   * no more ReadLog() are to be performed.
   *
   * NOTE: This method is not thread-safe.
   *
   */
  ExecutionResult ReadLog(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context) noexcept override;

  JournalId GetLastProcessedJournalId() noexcept override;

 protected:
  /**
   * @brief Reads the last checkpoint blob and returns the content on the
   * callback. The blob name is last_checkpoint within the storage provider.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ReadLastCheckpointBlob(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context) noexcept;

  /**
   * @brief When the read operation is completed on the last checkpoint blob,
   * this callback will be called.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void OnReadLastCheckpointBlobCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept;

  /**
   * @brief Reads any checkpoint blob and returns the contents on the callback.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param checkpoint_index The index of the checkpoint file.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ReadCheckpointBlob(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      CheckpointId checkpoint_id) noexcept;

  /**
   * @brief When the read operation is completed on the checkpoint blob,
   * this callback will be called.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void OnReadCheckpointBlobCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept;

  /**
   * @brief Reads any journal blob and returns the contents on the callback.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param journal_index Index of the journal file.
   * @param buffer_index Index of the buffer to be used to read the journals.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ReadJournalBlob(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      JournalId journal_id, size_t buffer_index) noexcept;

  /**
   * @brief When the read operation is completed on the journal blob,
   * this callback will be called.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param buffer_index Index of the buffer to be used to read the journals.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void OnReadJournalBlobCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context,
      size_t buffer_index) noexcept;

  /**
   * @brief Reads all journal blob provided by the input vector and returns the
   * contents on the callback.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param journal_ids Journal ids to be read.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ReadJournalBlobs(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          journal_stream_read_log_context,
      std::vector<JournalId>& journal_ids) noexcept;

  /**
   * @brief Lists all the existing checkpoints and returns the result on the
   * callback.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param start_from The blob to start the list operation from.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ListCheckpoints(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      std::shared_ptr<Blob>& start_from) noexcept;

  /**
   * @brief When the list checkpoints operation is completed this callback will
   * be called.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param list_blobs_context The context object of the list blobs operation.
   */
  virtual void OnListCheckpointsCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&
          list_blobs_context) noexcept;

  /**
   * @brief Lists all the existing journals and returns the result on the
   * callback.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param start_from The blob to start the list operation from.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ListJournals(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      std::shared_ptr<Blob>& start_from) noexcept;

  /**
   * @brief When the list journals operation is completed this callback will
   * be called.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @param list_blobs_context The context object of the list blobs operation.
   */
  virtual void OnListJournalsCallback(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context,
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&
          list_blobs_context) noexcept;

  /**
   * @brief Processes the loaded checkpoint (optional) and loaded journal logs
   * to produce a stream of logs and returns back to the caller's context.
   *
   * @param read_journal_input_stream_context The read journal input stream
   * context for the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ProcessLoadedJournals(
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          read_journal_input_stream_context) noexcept;

  /**
   * @brief Processes the next journal log line and updates the provided
   * arguments with the parsed data.
   *
   * @param timestamp The timestamp of the operation.
   * @param journal_log_status The log status of the operation.
   * @param component_id The component id of the owner of the id.
   * @param log_id The log id.
   * @param journal_log The serialized journal log.
   * @param journal_id The ID of journal from where the log is read
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ProcessNextJournalLog(
      core::Timestamp& timestamp, JournalLogStatus& journal_log_status,
      core::common::Uuid& component_id, core::common::Uuid& log_id,
      journal_service::JournalLog& journal_log, JournalId& journal_id) noexcept;

  /**
   * @brief Constructs a batch of journal logs based on the current journal
   * logs.
   *
   * @param journal_batch The output variable to write the batch to.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ReadJournalLogBatch(
      std::shared_ptr<std::vector<journal_service::JournalStreamReadLogObject>>&
          journal_batch) noexcept;

  /// Is set to true when all the journals are loaded in the memory.
  bool journals_loaded_;

  /// Is set to true when all journal IDs are loaded in the memory.
  bool journal_ids_loaded_ = false;

  /// Last stored checkpoint id to start the read operation from.
  CheckpointId last_checkpoint_id_;

  /// Last processed journal id by the previous checkpoint.
  JournalId last_processed_journal_id_;

  /// Total number of journal blobs to read. This is used as a counting
  /// semaphore to allow the last callback to execute the async sequence's
  /// continuation.
  std::atomic<size_t> total_journals_to_read_;

  /// All the journal ids to read.
  std::vector<JournalId> journal_ids_;

  /// Vector of all buffers to read the journals from.
  /// Journal buffers contains both checkpointed journal buffer and regular
  /// journal buffer(s).
  /// [[checkpointed log buffer (0..1)], ..., [journal log buffer(s) (0..N)]]
  std::vector<BytesBuffer> journal_buffers_;

  /// The execution result of the read journals operation.
  std::atomic<ExecutionResult> execution_result_of_failed_journal_read_;

  /// Whether any operation failed, if set to true, read journals operation will
  /// fail.
  std::atomic<bool> is_any_journal_read_failed_;

  /// When all the blobs are loaded, the stream need to keep track of its
  /// current position. The following two parameters are used for this purpose.
  size_t current_buffer_index_;
  size_t current_buffer_offset_;

  /// The current bucket name to write the blob to.
  std::shared_ptr<std::string> bucket_name_;

  /// The current partition name to write the blob to.
  std::shared_ptr<std::string> partition_name_;

  /// Blob storage provider client instance.
  std::shared_ptr<BlobStorageClientInterface> blob_storage_provider_client_;

 private:
  bool IsJournalBuffersLoadedButNotProcessedYet();

  JournalId GetCurrentBufferJournalId();

  std::shared_ptr<ConfigProviderInterface> config_provider_;

  size_t journal_ids_window_start_index_;
  size_t journal_ids_window_length_;

  bool enable_batch_read_journals_;
  size_t number_of_journals_per_batch_;
  size_t number_of_journal_logs_to_return_;
};
}  // namespace google::scp::core
