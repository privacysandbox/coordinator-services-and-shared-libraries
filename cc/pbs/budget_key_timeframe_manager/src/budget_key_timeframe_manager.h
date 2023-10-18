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
#include <string>
#include <vector>

#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kBudgetKeyTimeframeManagerRetryStrategyDelayMs = 31;
static constexpr size_t kBudgetKeyTimeframeManagerRetryStrategyTotalRetries =
    10;
static constexpr int kBudgetKeyTimeframeManagerCacheLifetimeSeconds = 120;

namespace google::scp::pbs {
/*! @copydoc BudgetKeyTimeframeManagerInterface
 */
class BudgetKeyTimeframeManager : public BudgetKeyTimeframeManagerInterface {
 public:
  BudgetKeyTimeframeManager(
      const std::shared_ptr<std::string>& budget_key_name,
      const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<cpio::AggregateMetricInterface>&
          budget_key_count_metric)
      : BudgetKeyTimeframeManager(budget_key_name, id, async_executor,
                                  journal_service, nosql_database_provider,
                                  nosql_database_provider, metric_client,
                                  config_provider, budget_key_count_metric) {}

  BudgetKeyTimeframeManager(
      const std::shared_ptr<std::string>& budget_key_name,
      const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider_for_background_operations,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider_for_live_traffic,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<cpio::AggregateMetricInterface>&
          budget_key_count_metric)
      : budget_key_name_(budget_key_name),
        id_(id),
        async_executor_(async_executor),
        journal_service_(journal_service),
        nosql_database_provider_for_background_operations_(
            nosql_database_provider_for_background_operations),
        nosql_database_provider_for_live_traffic_(
            nosql_database_provider_for_live_traffic),
        budget_key_timeframe_groups_(
            std::make_unique<core::common::AutoExpiryConcurrentMap<
                TimeBucket, std::shared_ptr<BudgetKeyTimeframeGroup>>>(
                kBudgetKeyTimeframeManagerCacheLifetimeSeconds,
                true /* extend_entry_lifetime_on_access */,
                true /* block_entry_while_eviction */,
                std::bind(&BudgetKeyTimeframeManager::OnBeforeGarbageCollection,
                          this, std::placeholders::_1, std::placeholders::_2,
                          std::placeholders::_3),
                async_executor)),
        operation_dispatcher_(
            async_executor,
            core::common::RetryStrategy(
                core::common::RetryStrategyType::Exponential,
                kBudgetKeyTimeframeManagerRetryStrategyDelayMs,
                kBudgetKeyTimeframeManagerRetryStrategyTotalRetries)),
        metric_client_(metric_client),
        config_provider_(config_provider),
        budget_key_count_metric_(budget_key_count_metric) {}

  ~BudgetKeyTimeframeManager();

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult CanUnload() noexcept override;

  core::ExecutionResult Load(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept override;

  core::ExecutionResult Update(
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept override;

  const core::common::Uuid GetId() noexcept override { return id_; }

  core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>& checkpoint_logs) noexcept
      override;

 protected:
  /**
   * @brief Helper function to populate budget key time frames in response
   * w.r.t. time buckets specified in the request
   *
   * @param budget_key_timeframe_group time frame group from which the response
   * time frames are populated
   * @param load_budget_key_timeframe_request request object
   * @param load_budget_key_timeframe_response response object
   * @return core::ExecutionResult
   */
  static core::ExecutionResult PopulateLoadBudgetKeyTimeframeResponse(
      const std::shared_ptr<BudgetKeyTimeframeGroup>&
          budget_key_timeframe_group,
      const std::shared_ptr<LoadBudgetKeyTimeframeRequest>&
          load_budget_key_timeframe_request,
      std::shared_ptr<LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_response);

  /**
   * @brief Is Called right before the map garbage collector is trying to
   * remove the element from the map.
   *
   * @param time_group The time group that is being removed.
   * @param budget_key_timeframe_group The budget key timeframe group object
   * that is being removed.
   * @param should_delete_entry Callback to allow/deny the garbage
   * collection request.
   */
  virtual void OnBeforeGarbageCollection(
      TimeGroup& time_group,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      std::function<void(bool)> should_delete_entry) noexcept;

  /**
   * @brief Is called when the removal operation is logged.
   *
   * @param should_delete_entry Callback to allow/deny the garbage collection
   * request.
   * @param journal_log_context The journal log context of the operation.
   */
  virtual void OnRemoveEntryFromCacheLogged(
      std::function<void(bool)> should_delete_entry,
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
   * @brief Is called when logging on the update operation is completed.
   *
   * @param update_budget_key_timeframe_context The update bucket key timeframe
   * operation context.
   * @param budget_key_timeframes The budget key timeframes of time group that
   * need to be modified
   * @param journal_log_context The journal log operation context.
   */
  virtual void OnLogUpdateCallback(
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context,
      std::vector<std::shared_ptr<BudgetKeyTimeframe>>& budget_key_timeframes,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept;

  /**
   * @brief Is called when logging on the load operation is completed.
   *
   * @param load_budget_key_timeframe_context The load time timeframe group
   * operation context.
   * @param budget_key_timeframe_group The budget key group that is loaded.
   * @param journal_log_context The journal log operation context.
   */
  virtual void OnLogLoadCallback(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept;

  /**
   * @brief Is called when the store budget key call to the database is
   * completed.
   *
   * @param upsert_database_item_context The upsert database item context.
   * @param time_group The time group of the operation.
   * @param budget_key_timeframe_group The pointer the budget key timeframe
   * group.
   * @param should_delete_entry Callback to allow/deny the garbage collection
   * request.
   */
  virtual void OnStoreTimeframeGroupToDBCallback(
      core::AsyncContext<core::UpsertDatabaseItemRequest,
                         core::UpsertDatabaseItemResponse>&
          upsert_database_item_context,
      TimeGroup& time_group,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      std::function<void(bool)> should_delete_entry) noexcept;

  /**
   * @brief Loads the specific timeframe group from the database.
   *
   * @param load_budget_key_timeframe_context The load budget key timeframe
   * context of the operation.
   * @param budget_key_timeframe_group The budget key time frame group to load
   * the data into.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult LoadTimeframeGroupFromDB(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context,
      std::shared_ptr<BudgetKeyTimeframeGroup>&
          budget_key_timeframe_group) noexcept;

  /**
   * @brief Is called when the load time frame group from DB operation is
   * completed.
   *
   * @param load_budget_key_timeframe_context The load budget key timeframe
   * context of the operation.
   * @param budget_key_timeframe_group The budget key time frame group to load
   * the data into.
   * @param get_database_item_context The get database item context of the
   * operation.
   */
  virtual void OnLoadTimeframeGroupFromDBCallback(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      core::AsyncContext<core::GetDatabaseItemRequest,
                         core::GetDatabaseItemResponse>&
          get_database_item_context) noexcept;

  /// The name of the budget parent key.
  const std::shared_ptr<std::string> budget_key_name_;

  /// The id of the budget key timeframe manager.
  const core::common::Uuid id_;

  /// An instance to the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance to the journal service.
  const std::shared_ptr<core::JournalServiceInterface> journal_service_;

  /// An instance to the nosql database provider.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_background_operations_;

  /// An instance to the nosql database provider.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_live_traffic_;

  /// The concurrent map of the budget key time frame groups.
  std::unique_ptr<core::common::AutoExpiryConcurrentMap<
      TimeGroup, std::shared_ptr<BudgetKeyTimeframeGroup>>>
      budget_key_timeframe_groups_;

  /// Operation dispatcher
  core::common::OperationDispatcher operation_dispatcher_;
  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  /// An instance of the config provider
  const std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// The aggregate metric instance for budget key counters
  std::shared_ptr<cpio::AggregateMetricInterface> budget_key_count_metric_;
};
}  // namespace google::scp::pbs
