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

#include <memory>
#include <string>

#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/partition_types.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/interface/pbs_partition_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {

/*! @copydoc PBSPartitionInterface
 */
class PBSPartition : public PBSPartitionInterface {
 public:
  struct Dependencies {
    /// @brief For all of background NoSQL operations that the partition
    /// generates
    std::shared_ptr<core::NoSQLDatabaseProviderInterface>
        nosql_database_provider_for_background_operations;
    /// @brief For all of foreground/live-traffic operations that the partition
    /// generates
    std::shared_ptr<core::NoSQLDatabaseProviderInterface>
        nosql_database_provider_for_live_traffic;
    /// @brief For all of Blob Store operations that the partition generates
    std::shared_ptr<core::BlobStorageProviderInterface> blob_store_provider;
    /// @brief For all of Blob Store operations that checkpointing service
    /// generates. Checkpoint service needs a seperate provider for isolation
    /// reasons.
    std::shared_ptr<core::BlobStorageProviderInterface>
        blob_store_provider_for_checkpoints;
    /// @brief For all compute operations of the partition
    std::shared_ptr<core::AsyncExecutorInterface> async_executor;
    /// @brief For configurations that partition needs
    std::shared_ptr<core::ConfigProviderInterface> config_provider;
    /// @brief For metrics that partitions generates
    std::shared_ptr<cpio::MetricClientInterface> metric_client;
    /// @brief For Transaction Resolution operations that need to talk to remote
    /// PBS
    std::shared_ptr<core::RemoteTransactionManagerInterface>
        remote_transaction_manager;
  };

  PBSPartition(const core::PartitionId& partition_id,
               const Dependencies& partition_dependencies,
               std::shared_ptr<std::string> partition_journal_bucket_name,
               size_t partition_transaction_manager_capacity);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Load() noexcept override;

  core::ExecutionResult Unload() noexcept override;

  core::PartitionLoadUnloadState GetPartitionState() noexcept override;

 public:
  core::ExecutionResult ExecuteRequest(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>& context) noexcept
      override;

  core::ExecutionResult ExecuteRequest(
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          context) noexcept override;

  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>& context) noexcept
      override;

  core::ExecutionResult GetTransactionManagerStatus(
      const core::GetTransactionManagerStatusRequest& request,
      core::GetTransactionManagerStatusResponse& response) noexcept override;

 protected:
  /**
   * @brief Perform Log Recovery on the partition synchronously.
   *
   * @return core::ExecutionResult
   */
  core::ExecutionResult RecoverPartition();

  void IncrementRequestCount();

  /// @brief checkpoint service for the partition
  std::shared_ptr<core::CheckpointServiceInterface> checkpoint_service_;

  /// @brief journal service for the partition
  std::shared_ptr<core::JournalServiceInterface> journal_service_;

  /// @brief budget key provider of the partition
  std::shared_ptr<BudgetKeyProviderInterface> budget_key_provider_;

  /// @brief transaction orchestrator of the partition
  std::shared_ptr<core::TransactionManagerInterface> transaction_manager_;

  /// @brief ID of the partition
  const core::PartitionId partition_id_;

  /// @brief current load/unload state of the partition
  std::atomic<core::PartitionLoadUnloadState> partition_state_;

  /// @brief bucket at which partition's journal files are available or written
  std::shared_ptr<std::string> partition_journal_bucket_name_;

  /// @brief the number of maximum transactions that transaction manager can
  /// handle
  size_t partition_transaction_manager_capacity_;

  /// @brief external dependencies.
  Dependencies partition_dependencies_;

  /// @brief Requests seen since the partition startup. This is a relaxed
  /// counter which becomes eventually consistent and should be used only for
  /// approximate calculations.
  std::atomic<size_t> requests_seen_count_;
};

};  // namespace google::scp::pbs
