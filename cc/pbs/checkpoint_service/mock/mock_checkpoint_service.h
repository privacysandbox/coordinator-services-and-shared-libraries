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

#include <functional>
#include <memory>
#include <string>

#include "pbs/checkpoint_service/src/checkpoint_service.h"

namespace google::scp::pbs::checkpoint_service::mock {
class MockCheckpointService : public CheckpointService {
 public:
  MockCheckpointService(
      std::shared_ptr<std::string>& bucket_name,
      std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::BlobStorageProviderInterface>&
          blob_storage_provider,
      size_t initial_buffer_size)
      : CheckpointService(bucket_name, partition_name, metric_client,
                          config_provider, journal_service,
                          blob_storage_provider, initial_buffer_size) {}

  std::function<core::ExecutionResult()> bootstrap_mock;
  std::function<core::ExecutionResult()> shutdown_mock;
  std::function<core::ExecutionResult(core::JournalId&)> recover_mock;
  std::function<core::ExecutionResult(core::JournalId last_processed_journal_id,
                                      core::CheckpointId& checkpoint_id,
                                      core::BytesBuffer& last_checkpoint_buffer,
                                      core::BytesBuffer& checkpoint_buffer)>
      checkpoint_mock;
  std::function<core::ExecutionResult(
      std::shared_ptr<core::BlobStorageClientInterface>& blob_storage_client,
      std::shared_ptr<std::string>& blob_name,
      std::shared_ptr<core::BytesBuffer>& bytes_buffer)>
      write_blob_mock;

  std::function<core::ExecutionResult(core::CheckpointId& checkpoint_id,
                                      core::BytesBuffer& last_checkpoint_buffer,
                                      core::BytesBuffer& checkpoint_buffer)>
      store_mock;

  virtual core::ExecutionResult RunCheckpointWorker() noexcept {
    return CheckpointService::RunCheckpointWorker();
  }

  virtual core::ExecutionResult Bootstrap() noexcept {
    if (bootstrap_mock) {
      return bootstrap_mock();
    }
    return CheckpointService::Bootstrap();
  }

  virtual core::ExecutionResult Recover(
      core::JournalId& last_processed_journal_id) noexcept {
    if (recover_mock) {
      return recover_mock(last_processed_journal_id);
    }

    return CheckpointService::Recover(last_processed_journal_id);
  }

  virtual core::ExecutionResult Checkpoint(
      core::JournalId last_processed_journal_id,
      core::CheckpointId& checkpoint_id,
      core::BytesBuffer& last_checkpoint_buffer,
      core::BytesBuffer& checkpoint_buffer) noexcept {
    if (checkpoint_mock) {
      return checkpoint_mock(last_processed_journal_id, checkpoint_id,
                             last_checkpoint_buffer, checkpoint_buffer);
    }
    return CheckpointService::Checkpoint(last_processed_journal_id,
                                         checkpoint_id, last_checkpoint_buffer,
                                         checkpoint_buffer);
  }

  virtual core::ExecutionResult WriteBlob(
      std::shared_ptr<core::BlobStorageClientInterface>& blob_storage_client,
      std::shared_ptr<std::string>& blob_name,
      std::shared_ptr<core::BytesBuffer>& bytes_buffer) {
    if (write_blob_mock) {
      return write_blob_mock(blob_storage_client, blob_name, bytes_buffer);
    }
    return CheckpointService::WriteBlob(blob_storage_client, blob_name,
                                        bytes_buffer);
  }

  core::ExecutionResult Store(core::CheckpointId& checkpoint_id,
                              core::BytesBuffer& last_checkpoint_buffer,
                              core::BytesBuffer& checkpoint_buffer) noexcept {
    if (store_mock) {
      return store_mock(checkpoint_id, last_checkpoint_buffer,
                        checkpoint_buffer);
    }
    return CheckpointService::Store(checkpoint_id, last_checkpoint_buffer,
                                    checkpoint_buffer);
  }

  virtual core::ExecutionResult Shutdown() noexcept {
    if (shutdown_mock) {
      return shutdown_mock();
    }
    return CheckpointService::Shutdown();
  }

  void SetAsyncExecutor(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor) {
    async_executor_ = async_executor;
  }

  void SetBlobStorageProvider(
      std::shared_ptr<core::BlobStorageProviderInterface>
          blob_storage_provider) {
    blob_storage_provider_ = blob_storage_provider;
  }

  void SetJournalService(
      std::shared_ptr<core::JournalServiceInterface> journal_service) {
    journal_service_ = journal_service;
  }

  void SetBudgetKeyProvider(
      std::shared_ptr<pbs::BudgetKeyProviderInterface> budget_key_provider) {
    budget_key_provider_ = budget_key_provider;
  }

  void SetTransactionManager(
      std::shared_ptr<core::TransactionManagerInterface> transaction_manager) {
    transaction_manager_ = transaction_manager;
  }

  void SetJournalId(core::JournalId id) { last_processed_journal_id_ = id; }
};
}  // namespace google::scp::pbs::checkpoint_service::mock
