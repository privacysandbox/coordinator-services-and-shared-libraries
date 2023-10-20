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

#include <stddef.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/partition_types.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "core/interface/type_def.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

static constexpr size_t kCheckpointInitialBufferSize =
    512 * 1024 * 1024;  // 512 MB

namespace google::scp::pbs {
/*! @copydoc CheckpointServiceInterface
 */
class CheckpointService : public core::CheckpointServiceInterface {
 public:
  CheckpointService(
      std::shared_ptr<std::string>& bucket_name,
      std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<core::JournalServiceInterface>&
          application_journal_service,
      const std::shared_ptr<core::BlobStorageProviderInterface>&
          blob_storage_provider,
      size_t initial_buffer_size = kCheckpointInitialBufferSize)
      : is_running_(false),
        bucket_name_(bucket_name),
        partition_name_(partition_name),
        last_processed_journal_id_(0),
        last_persisted_checkpoint_id_(0),
        metric_client_(metric_client),
        initial_buffer_size_(initial_buffer_size),
        config_provider_(config_provider),
        application_journal_service_(application_journal_service),
        blob_storage_provider_(blob_storage_provider),
        checkpointing_interval_in_seconds_(0),
        max_journals_to_process_in_each_checkpoint_run_(0) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResultOr<core::CheckpointId>
  GetLastPersistedCheckpointId() noexcept override;

 protected:
  /// Create all needed components.
  virtual void CreateComponents() noexcept;

  /**
   * @brief Runs the main checkpointing logic.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult RunCheckpointWorker() noexcept;

  /**
   * @brief Initializes and runs all the underlying components.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Bootstrap() noexcept;

  /**
   * @brief Recovers the logs and sets the last processed journal id.
   *
   * @param last_processed_journal_id The id of the last journal log that was
   * recovered.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Recover(
      core::JournalId& last_processed_journal_id) noexcept;

  /**
   * @brief Performs the checkpointing operation and provides the buffers to
   * write to files.
   *
   * @param last_processed_journal_id The last processed journal id.
   * @param checkpoint_id The checkpoint id to be created.
   * @param last_checkpoint_buffer The last checkpoint file contents.
   * @param checkpoint_buffer The current checkpoint file content.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Checkpoint(
      core::JournalId last_processed_journal_id,
      core::CheckpointId& checkpoint_id,
      core::BytesBuffer& last_checkpoint_buffer,
      core::BytesBuffer& checkpoint_buffer) noexcept;

  /**
   * @brief Writes a blob into the blob storage service.
   *
   * @param blob_storage_client The blob storage client.
   * @param blob_name The blob name to write to.
   * @param bytes_buffer The bytes to be written to the blob.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult WriteBlob(
      std::shared_ptr<core::BlobStorageClientInterface>& blob_storage_client,
      std::shared_ptr<std::string>& blob_name,
      std::shared_ptr<core::BytesBuffer>& bytes_buffer);

  /**
   * @brief Stores the checkpoint and last_checkpoint blob.
   *
   * @param checkpoint_id The checkpoint id to be written.
   * @param last_checkpoint_buffer The last checkpoint data to be written.
   * @param checkpoint_buffer The checkpoint data to be written.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Store(
      core::CheckpointId& checkpoint_id,
      core::BytesBuffer& last_checkpoint_buffer,
      core::BytesBuffer& checkpoint_buffer) noexcept;

  /**
   * @brief Shuts down all the components and then nullifies the pointers.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Shutdown() noexcept;

  /// The checkpointing worker thread.
  std::thread worker_thread_;
  /// Indicates whether the checkpoint service is running.
  std::atomic<bool> is_running_;
  /// The bucket name of the current partition.
  std::shared_ptr<std::string> bucket_name_;
  /// The name of the partition.
  std::shared_ptr<std::string> partition_name_;
  /// The last processed journal log id;
  core::JournalId last_processed_journal_id_;
  /// The last persisted checkpoint id;
  core::CheckpointId last_persisted_checkpoint_id_;
  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  /// The initial buffers size to write the blobs.
  size_t initial_buffer_size_;
  /// An instance of the async executor for the IO operations.
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  /// An instance of the journal service.
  std::shared_ptr<core::JournalServiceInterface> journal_service_;
  /// An instance of the nosql database provider.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_;
  /// An instance of the budget key provider.
  std::shared_ptr<pbs::BudgetKeyProviderInterface> budget_key_provider_;
  /// An instance of the transaction command serializer.
  std::shared_ptr<core::TransactionCommandSerializerInterface>
      transaction_command_serializer_;
  /// An instance of the transaction manager.
  std::shared_ptr<core::TransactionManagerInterface> transaction_manager_;
  /// An instance of the remote transaction manager.
  std::shared_ptr<core::RemoteTransactionManagerInterface>
      remote_transaction_manager_;
  /// An instance of the config provider.
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  /// An instance of the application journal service. This is required for
  /// getting the latest journal id written to the storage provider.
  std::shared_ptr<core::JournalServiceInterface> application_journal_service_;
  /// The current activity id of the checkpoint service.
  core::common::Uuid activity_id_;
  /// An instance of the blob storage provider.
  std::shared_ptr<core::BlobStorageProviderInterface> blob_storage_provider_;
  /// Time between checkpointing runs.
  size_t checkpointing_interval_in_seconds_;
  /// Maximum number of journal entries to process in each checkpointing run.
  size_t max_journals_to_process_in_each_checkpoint_run_;
  /// Encapsulating partition ID
  core::PartitionId partition_id_;
};
}  // namespace google::scp::pbs
