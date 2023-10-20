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
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/journal_service/interface/journal_service_stream_interface.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

namespace google::scp::core {
/*! @copydoc JournalOutputStreamInterface
 */
class JournalOutputStream
    : public journal_service::JournalOutputStreamInterface {
 public:
  JournalOutputStream(
      const std::shared_ptr<std::string>& bucket_name,
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<BlobStorageClientInterface>&
          blob_storage_provider_client,
      const std::shared_ptr<cpio::AggregateMetricInterface>&
          journal_output_metric);

  ExecutionResult AppendLog(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          journal_stream_append_log_context) noexcept override;

  ExecutionResult GetLastPersistedJournalId(
      JournalId& journal_id) noexcept override;

  ExecutionResult FlushLogs() noexcept override;

 protected:
  /**
   * @brief Atomically creates a new buffer object.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult CreateNewBuffer() noexcept;

  /**
   * @brief Calculates the size of the serialized version of the current log.
   *
   * @param journal_stream_append_log_context The journal stream append log
   * context.
   * @return size_t Calculated size of the serialization.
   */
  virtual size_t GetSerializedLogByteSize(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          journal_stream_append_log_context) noexcept;

  /**
   * @brief Serializes the log object provided by the journal stream append log
   * context object.
   *
   * @param journal_stream_append_log_context The journal stream append log
   * context.
   * @param buffer The buffer to serialize the object to.
   * @param bytes_serialized The total bytes serialized.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult SerializeLog(
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          journal_stream_append_log_context,
      BytesBuffer& buffer, size_t& bytes_serialized) noexcept;

  /**
   * @brief Writes the provided buffer to the journal blob with the provided id.
   *
   * @param bytes_buffer The bytes buffer to write to the blob.
   * @param current_journal_id The journal id to create the blob.
   * @param callback The callback to call when the operation is completed.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult WriteJournalBlob(
      BytesBuffer& bytes_buffer, uint64_t current_journal_id,
      std::function<void(ExecutionResult&)> callback) noexcept;

  /**
   * @brief Called when write operation on the journal blob is compeleted.
   *
   * @param journal_id The journal id of the written blob.
   * @param callback The callback to call when the operation is completed.
   * @param pub_blob_context The context of the put blob operation.
   */
  virtual void OnWriteJournalBlobCallback(
      uint64_t journal_id, std::function<void(ExecutionResult&)> callback,
      AsyncContext<PutBlobRequest, PutBlobResponse>& pub_blob_context) noexcept;

  /**
   * @brief Notifies all the async context objects waiting on the current batch
   * to be flushed.
   *
   * @param flush_batch The batch of the async contexts.
   * @param execution_result The execution result of the upload operation.
   */
  virtual void NotifyBatch(
      const std::shared_ptr<std::list<core::AsyncContext<
          journal_service::JournalStreamAppendLogRequest,
          journal_service::JournalStreamAppendLogResponse>>>& flush_batch,
      ExecutionResult& execution_result) noexcept;

  /**
   * @brief Writes a single batch to the cloud storage provider.
   *
   * @param flush_batch The batch of the async contexts.
   * @param journal_id The current journal id for the batch.
   */
  virtual void WriteBatch(
      const std::shared_ptr<std::list<core::AsyncContext<
          journal_service::JournalStreamAppendLogRequest,
          journal_service::JournalStreamAppendLogResponse>>>& flush_batch,
      JournalId journal_id) noexcept;

  /// Mutex to synchronize concurrent batch creations of the pending logs.
  std::mutex create_batch_of_logs_mutex_;

  /// The current journal id to write the buffers to.
  JournalId current_journal_id_;

  /// The current bucket name to write the blob to.
  std::shared_ptr<std::string> bucket_name_;

  /// The current partition name to write the blob to.
  std::shared_ptr<std::string> partition_name_;

  /// Async executor instance.
  std::shared_ptr<AsyncExecutorInterface> async_executor_;

  /// Blob storage provider client instance.
  std::shared_ptr<BlobStorageClientInterface> blob_storage_provider_client_;

  /// The aggregate metric instance for journal output count
  std::shared_ptr<cpio::AggregateMetricInterface> journal_output_count_metric_;

  /// The last persisted journal id by the writer.
  JournalId last_persisted_journal_id_;

  /// The last successfully flushed journal index is very important for the
  /// checkpoint service to avoid skipping journal files. If value here
  /// indicates whether there is a pending write operation exist for the
  /// associated journal id. True means completed.
  core::common::ConcurrentMap<JournalId, std::shared_ptr<bool>>
      journals_to_persist_;

  /// The number of pending logs to be flushed.
  std::atomic<size_t> pending_logs_;

  /// Logs queue to create batches from.
  core::common::ConcurrentQueue<
      core::AsyncContext<journal_service::JournalStreamAppendLogRequest,
                         journal_service::JournalStreamAppendLogResponse>>
      logs_queue_;

  /// Parent activity ID for contexts/operations in this class.
  core::common::Uuid activity_id_;
};
}  // namespace google::scp::core
