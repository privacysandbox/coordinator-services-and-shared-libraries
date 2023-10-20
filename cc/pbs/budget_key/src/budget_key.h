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

#include <list>
#include <memory>

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/operation_dispatcher/src/retry_strategy.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kBudgetKeyRetryStrategyDelayMs = 31;
static constexpr size_t kBudgetKeyRetryStrategyTotalRetries = 12;

namespace google::scp::pbs {

/*! @copydoc BudgetKeyInterface
 */
class BudgetKey : public BudgetKeyInterface {
 public:
  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  /**
   * @brief Constructs a new Budget Key object using the name and initializes
   * the other internal properties automatically.
   *
   * @param name budget key name
   * @param id budget key ID
   * @param async_executor
   * @param journal_service
   * @param nosql_database_provider
   * @param metric_client
   * @param config_provider
   * @param budget_key_count_metric Metric to keep count of Budget Key stats.
   */
  BudgetKey(
      const std::shared_ptr<BudgetKeyName>& name, const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<cpio::AggregateMetricInterface>&
          budget_key_count_metric)
      : BudgetKey(name, id, async_executor, journal_service,
                  nosql_database_provider, nosql_database_provider,
                  metric_client, config_provider, budget_key_count_metric) {
    // This construction does not make any distinction between background and
    // live traffic NoSQL operations.
  }

  /**
   * @brief Constructs a new Budget Key object using the name and initializes
   * the other internal properties automatically.
   *
   * @param name budget key name
   * @param id budget key ID
   * @param async_executor
   * @param journal_service
   * @param nosql_database_provider_for_background_operations
   * @param nosql_database_provider_for_live_traffic
   * @param metric_client
   * @param config_provider
   * @param budget_key_count_metric Metric to keep count of Budget Key stats.
   */
  BudgetKey(
      const std::shared_ptr<BudgetKeyName>& name, const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider_for_background_operations,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider_for_live_traffic,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<cpio::AggregateMetricInterface>&
          budget_key_count_metric);

  /**
   * @brief Constructs a new Budget Key object using customized timeframe
   * manager.
   *
   * @param name budget key name
   * @param id budget key ID
   * @param async_executor
   * @param journal_service
   * @param nosql_database_provider
   * @param budget_key_timeframe_manager
   * @param consume_budget_transaction_protocol
   * @param metric_client
   * @param config_provider
   * @param budget_key_count_metric Metric to keep count of Budget Key stats.
   */
  BudgetKey(
      const std::shared_ptr<BudgetKeyName>& name, const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      std::shared_ptr<core::NoSQLDatabaseProviderInterface>
          nosql_database_provider,
      const std::shared_ptr<BudgetKeyTimeframeManagerInterface>&
          budget_key_timeframe_manager,
      const std::shared_ptr<ConsumeBudgetTransactionProtocolInterface>&
          consume_budget_transaction_protocol,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<cpio::AggregateMetricInterface>&
          budget_key_count_metric)
      : BudgetKey(name, id, async_executor, journal_service,
                  nosql_database_provider, nosql_database_provider,
                  budget_key_timeframe_manager,
                  consume_budget_transaction_protocol, metric_client,
                  config_provider, budget_key_count_metric) {}

  /**
   * @brief Constructs a new Budget Key object using customized timeframe
   * manager.
   *
   * @param name budget key name
   * @param id budget key ID
   * @param async_executor
   * @param journal_service
   * @param nosql_database_provider_for_background_operations
   * @param nosql_database_provider_for_live_traffic
   * @param budget_key_timeframe_manager
   * @param consume_budget_transaction_protocol
   * @param metric_client
   * @param config_provider
   * @param budget_key_count_metric Metric to keep count of Budget Key
   * stats.
   */
  BudgetKey(
      const std::shared_ptr<BudgetKeyName>& name, const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      std::shared_ptr<core::NoSQLDatabaseProviderInterface>
          nosql_database_provider_for_background_operations,
      std::shared_ptr<core::NoSQLDatabaseProviderInterface>
          nosql_database_provider_for_live_traffic,
      const std::shared_ptr<BudgetKeyTimeframeManagerInterface>&
          budget_key_timeframe_manager,
      const std::shared_ptr<ConsumeBudgetTransactionProtocolInterface>&
          consume_budget_transaction_protocol,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<cpio::AggregateMetricInterface>&
          budget_key_count_metric)
      : name_(name),
        id_(id),
        async_executor_(async_executor),
        journal_service_(journal_service),
        nosql_database_provider_for_background_operations_(
            nosql_database_provider_for_background_operations),
        nosql_database_provider_for_live_traffic_(
            nosql_database_provider_for_live_traffic),
        budget_key_timeframe_manager_(budget_key_timeframe_manager),
        consume_budget_transaction_protocol_(
            consume_budget_transaction_protocol),
        operation_dispatcher_(async_executor,
                              core::common::RetryStrategy(
                                  core::common::RetryStrategyType::Exponential,
                                  kBudgetKeyRetryStrategyDelayMs,
                                  kBudgetKeyRetryStrategyTotalRetries)),
        metric_client_(metric_client),
        config_provider_(config_provider),
        budget_key_count_metric_(budget_key_count_metric) {}

  ~BudgetKey();

  core::ExecutionResult CanUnload() noexcept override;

  core::ExecutionResult LoadBudgetKey(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context) noexcept override;

  core::ExecutionResult GetBudget(
      core::AsyncContext<GetBudgetRequest, GetBudgetResponse>&
          get_budget_context) noexcept override;

  const std::shared_ptr<ConsumeBudgetTransactionProtocolInterface>
  GetBudgetConsumptionTransactionProtocol() noexcept override {
    return consume_budget_transaction_protocol_;
  }

  const std::shared_ptr<BatchConsumeBudgetTransactionProtocolInterface>
  GetBatchBudgetConsumptionTransactionProtocol() noexcept override {
    return batch_consume_budget_transaction_protocol_;
  }

  const std::shared_ptr<BudgetKeyName> GetName() noexcept override {
    return name_;
  }

  const core::common::Uuid GetId() noexcept override { return id_; }

  core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>& checkpoint_logs) noexcept
      override;

 protected:
  /**
   * @brief Serializes the budget key into the provided buffer.
   *
   * @param budget_key_timeframe_manager_id
   * @param budget_key_log_bytes_buffer
   * @return core::ExecutionResult
   */
  core::ExecutionResult SerializeBudgetKey(
      core::common::Uuid& budget_key_timeframe_manager_id,
      core::BytesBuffer& budget_key_log_bytes_buffer) noexcept;

  /**
   * @brief Is called when the journal service has returned with the journal log
   * callback.
   *
   * @param load_budget_key_context The load budget key context object.
   * @param budget_key_timeframe_manager_id The budget key time frame manager id
   * to load.
   * @param journal_log_context The journal log operation context.
   */
  virtual void OnLogLoadBudgetKeyCallback(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context,
      core::common::Uuid& budget_key_timeframe_manager_id,
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
   * @brief Notification method called once the key value has been loaded from
   * the database.
   *
   * @param get_budget_context The original get budget operation context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnBudgetLoaded(core::AsyncContext<GetBudgetRequest, GetBudgetResponse>&
                          get_budget_context,
                      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                                         LoadBudgetKeyTimeframeResponse>&
                          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Get the timeframe manager id for the budget key.
   *
   * @return core::common::Uuid The timeframe manager id.
   */
  core::common::Uuid GetTimeframeManagerId() noexcept;

  /// The name of the current budget key.
  const std::shared_ptr<BudgetKeyName> name_;

  /// The id of the current budget key.
  const core::common::Uuid id_;

  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the journal service.
  std::shared_ptr<core::JournalServiceInterface> journal_service_;

  /// An instance to the nosql database provider.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_background_operations_;

  /// An instance to the nosql database provider.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_live_traffic_;

  /// The budget key frame manager.
  std::shared_ptr<BudgetKeyTimeframeManagerInterface>
      budget_key_timeframe_manager_;

  /// Transaction protocol for budget consumption.
  std::shared_ptr<ConsumeBudgetTransactionProtocolInterface>
      consume_budget_transaction_protocol_;

  /// Transaction protocol for batch budget consumption.
  std::shared_ptr<BatchConsumeBudgetTransactionProtocolInterface>
      batch_consume_budget_transaction_protocol_;

  /// Operation dispatcher
  core::common::OperationDispatcher operation_dispatcher_;
  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  /// An instance of the config provider.
  const std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// The aggregate metric instance for budget key counters
  std::shared_ptr<cpio::AggregateMetricInterface> budget_key_count_metric_;
};
}  // namespace google::scp::pbs
