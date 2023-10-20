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
#include <utility>

#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/operation_dispatcher/src/retry_strategy.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/partition_types.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/budget_key_provider/src/proto/budget_key_provider.pb.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kBudgetKeyProviderRetryStrategyDelayMs = 31;
static constexpr size_t kBudgetKeyProviderRetryStrategyTotalRetries = 12;
static constexpr int kBudgetKeyProviderCacheLifetimeSeconds = 300;

namespace google::scp::pbs {
/// Stores budget_key and associated loading status.
struct BudgetKeyProviderPair : public core::LoadableObject {
  /// A pointer to the budget key.
  std::shared_ptr<BudgetKeyInterface> budget_key;
};

/*! @copydoc BudgetKeyProviderInterface
 */
class BudgetKeyProvider : public BudgetKeyProviderInterface {
 public:
  BudgetKeyProvider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const core::PartitionId& partition_id = core::kGlobalPartitionId)
      : BudgetKeyProvider(async_executor, journal_service,
                          nosql_database_provider, nosql_database_provider,
                          metric_client, config_provider, partition_id) {
    // This construction does not make any distinction between background and
    // live traffic NoSQL operations.
  }

  BudgetKeyProvider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider_for_background_operations,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider_for_live_traffic,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const core::PartitionId& partition_id = core::kGlobalPartitionId)
      : async_executor_(async_executor),
        journal_service_(journal_service),
        nosql_database_provider_for_background_operations_(
            nosql_database_provider_for_background_operations),
        nosql_database_provider_for_live_traffic_(
            nosql_database_provider_for_live_traffic),
        budget_keys_(std::make_unique<core::common::AutoExpiryConcurrentMap<
                         std::string, std::shared_ptr<BudgetKeyProviderPair>>>(
            kBudgetKeyProviderCacheLifetimeSeconds,
            true /* extend_entry_lifetime_on_access */,
            true /* block_entry_while_eviction */,
            std::bind(&BudgetKeyProvider::OnBeforeGarbageCollection, this,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3),
            async_executor)),
        operation_dispatcher_(async_executor,
                              core::common::RetryStrategy(
                                  core::common::RetryStrategyType::Exponential,
                                  kBudgetKeyProviderRetryStrategyDelayMs,
                                  kBudgetKeyProviderRetryStrategyTotalRetries)),
        metric_client_(metric_client),
        config_provider_(config_provider),
        partition_id_(partition_id),
        activity_id_(core::common::Uuid::GenerateUuid()) {}

  /**
   * @brief Construct a new Budget Key Provider object for Checkpoint Service
   * ONLY! Checkpointing service does not read/write any new data so it doesn't
   * need database provider, but needs journal_service to recover and
   * checkpoint existing logs.
   *
   * @param async_executor
   * @param journal_service
   * @param metric_client
   * @param config_provider
   */
  BudgetKeyProvider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const core::PartitionId& partition_id = core::kGlobalPartitionId)
      : BudgetKeyProvider(
            async_executor, journal_service,
            nullptr /* nosql_provider unused in checkpointing context */,
            nullptr /* nosql_provider unused in checkpointing context */,
            metric_client, config_provider, partition_id) {}

  ~BudgetKeyProvider();

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetBudgetKey(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context) noexcept override;

  core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>& checkpoint_logs) noexcept
      override;

 protected:
  /**
   * @brief Serializes the budget key provider pair to the provided buffer.
   *
   * @param budget_key_provider_pair The budget key provider pair to serialize.
   * @param operation_type The operation type, insert or remove from cache.
   * @param budget_key_provider_log_bytes_buffer The output buffer to be used
   * for serialization.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult SerializeBudgetKeyProviderPair(
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      budget_key_provider::proto::OperationType operation_type,
      core::BytesBuffer& budget_key_provider_log_bytes_buffer) noexcept;

  /**
   * @brief Is Called right before the map garbage collector is trying to remove
   * the element from the map.
   *
   * @param budget_key_name The budget key name that is being removed.
   * @param budget_key_pair The budget key pair that is being removed.
   * @param should_delete_entry Callback to allow/deny the garbage collection
   * request.
   */
  virtual void OnBeforeGarbageCollection(
      std::string& budget_key_name,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_pair,
      std::function<void(bool)> should_delete_entry) noexcept;

  /**
   * @brief Is called when the removal operation is logged.
   *
   * @param should_delete_entry Callback to allow/deny the garbage collection
   * request.
   * @param budget_key_pair The budget key pair that is being removed.
   * @param journal_log_context The journal log context of the operation.
   */
  virtual void OnRemoveEntryFromCacheLogged(
      std::function<void(bool)> should_delete_entry,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_pair,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept;

  /**
   * @brief The callback from the journal service to provide restored logs.
   *
   * @param bytes_buffer The bytes buffer containing the serialized logs.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult OnJournalServiceRecoverCallback(
      const std::shared_ptr<core::BytesBuffer>& bytes_buffer,
      const core::common::Uuid& activity_id) noexcept;

  /**
   * @brief Is called when budget key is loading, before entering into the
   * cache.
   *
   * @param get_budget_key_context The get budget key operation context.
   * @param budget_key_provider_pair The budget key provider pair containing the
   * budget key pointer and loading status.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult LogLoadBudgetKeyIntoCache(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      std::shared_ptr<BudgetKeyProviderPair>&
          budget_key_provider_pair) noexcept;

  /**
   * @brief Is called when the logging of the load budget key into cache is
   * completed.
   *
   * @param get_budget_key_context The get budget key operation context.
   * @param budget_key_provider_pair The budget key provider pair containing the
   * budget key pointer and loading status.
   * @param journal_log_context The context of the journal log operation.
   */
  virtual void OnLogLoadBudgetKeyIntoCacheCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept;

  /**
   * @brief Is called when the load budget key operation is completed.
   *
   * @param get_budget_key_context The get budget key operation context.
   * @param budget_key_provider_pair The budget key provider pair containing the
   * budget key pointer and loading status.
   * @param load_budget_key_context The context of the load budget key
   * operation.
   */
  virtual void OnLoadBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context) noexcept;

  /// An instance to the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance to the journal service.
  std::shared_ptr<core::JournalServiceInterface> journal_service_;

  /// An instance to the nosql database provider.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_background_operations_;

  /// An instance to the nosql database provider.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_live_traffic_;

  /**
   * @brief  Concurrent map to map budget key names to the budget keys. This
   * will be used as the cache of budget keys loaded from the DB
   */
  std::unique_ptr<core::common::AutoExpiryConcurrentMap<
      std::string, std::shared_ptr<BudgetKeyProviderPair>>>
      budget_keys_;

  /// Operation distpatcher
  core::common::OperationDispatcher operation_dispatcher_;

  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  /// An instance of the config provider;
  const std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// The aggregate metric instance for counting load/unload of budget keys
  std::shared_ptr<cpio::AggregateMetricInterface> budget_key_count_metric_;

  /// Encapsulating partition
  const core::PartitionId partition_id_;

  /// Activity ID
  const core::common::Uuid activity_id_;
};
}  // namespace google::scp::pbs
