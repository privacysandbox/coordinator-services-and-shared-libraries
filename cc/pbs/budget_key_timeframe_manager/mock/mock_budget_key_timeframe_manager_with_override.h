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
#include <string>
#include <vector>

#include "core/common/auto_expiry_concurrent_map/mock/mock_auto_expiry_concurrent_map.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_manager.h"
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

namespace google::scp::pbs::buget_key_timeframe_manager::mock {
class MockBudgetKeyTimeframeManager : public BudgetKeyTimeframeManager {
 public:
  MockBudgetKeyTimeframeManager(
      const std::shared_ptr<std::string>& budget_key_name,
      const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : BudgetKeyTimeframeManager(
            budget_key_name, id, async_executor, journal_service,
            nosql_database_provider, metric_client, config_provider,
            std::make_shared<cpio::MockAggregateMetric>()) {
    budget_key_timeframe_groups_ = std::make_unique<
        core::common::auto_expiry_concurrent_map::mock::
            MockAutoExpiryConcurrentMap<
                TimeGroup, std::shared_ptr<BudgetKeyTimeframeGroup>>>(
        100, true /* extend_entry_lifetime_on_access */,
        true /* block_entry_while_eviction */,
        std::bind(&MockBudgetKeyTimeframeManager::OnBeforeGarbageCollection,
                  this, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3),
        async_executor);
  }

  virtual core::ExecutionResult OnJournalServiceRecoverCallback(
      const std::shared_ptr<core::BytesBuffer>& bytes_buffer,
      const core::common::Uuid& activity_id) noexcept {
    return BudgetKeyTimeframeManager::OnJournalServiceRecoverCallback(
        bytes_buffer, activity_id);
  }

  core::ExecutionResult LoadTimeframeGroupFromDB(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context,
      std::shared_ptr<BudgetKeyTimeframeGroup>&
          budget_key_timeframe_group) noexcept {
    if (load_timeframe_group_from_db_mock) {
      return load_timeframe_group_from_db_mock(
          load_budget_key_timeframe_context, budget_key_timeframe_group);
    }

    return BudgetKeyTimeframeManager::LoadTimeframeGroupFromDB(
        load_budget_key_timeframe_context, budget_key_timeframe_group);
  }

  void OnLoadTimeframeGroupFromDBCallback(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      core::AsyncContext<core::GetDatabaseItemRequest,
                         core::GetDatabaseItemResponse>&
          get_database_item_context) noexcept {
    if (on_load_timeframe_group_from_db_mock_callback_mock) {
      on_load_timeframe_group_from_db_mock_callback_mock(
          load_budget_key_timeframe_context, budget_key_timeframe_group,
          get_database_item_context);
      return;
    }

    BudgetKeyTimeframeManager::OnLoadTimeframeGroupFromDBCallback(
        load_budget_key_timeframe_context, budget_key_timeframe_group,
        get_database_item_context);
  }

  void OnLogLoadCallback(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept {
    BudgetKeyTimeframeManager::OnLogLoadCallback(
        load_budget_key_timeframe_context, budget_key_timeframe_group,
        journal_log_context);
  }

  void OnLogUpdateCallback(
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context,
      std::vector<std::shared_ptr<BudgetKeyTimeframe>>& budget_key_timeframes,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept {
    BudgetKeyTimeframeManager::OnLogUpdateCallback(
        update_budget_key_timeframe_context, budget_key_timeframes,
        journal_log_context);
  }

  void OnBeforeGarbageCollection(
      TimeGroup& time_group,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      std::function<void(bool)> should_delete_entry) noexcept {
    BudgetKeyTimeframeManager::OnBeforeGarbageCollection(
        time_group, budget_key_timeframe_group, should_delete_entry);
  }

  void OnRemoveEntryFromCacheLogged(
      std::function<void(bool)> should_delete_entry,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept {
    BudgetKeyTimeframeManager::OnRemoveEntryFromCacheLogged(
        should_delete_entry, journal_log_context);
  }

  void OnStoreTimeframeGroupToDBCallback(
      core::AsyncContext<core::UpsertDatabaseItemRequest,
                         core::UpsertDatabaseItemResponse>&
          upsert_database_item_context,
      TimeGroup& time_group,
      std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
      std::function<void(bool)> should_delete_entry) noexcept {
    BudgetKeyTimeframeManager::OnStoreTimeframeGroupToDBCallback(
        upsert_database_item_context, time_group, budget_key_timeframe_group,
        should_delete_entry);
  }

  auto& GetBudgetTimeframeGroups() { return budget_key_timeframe_groups_; }

  auto* GetInternalBudgetTimeframeGroups() {
    return static_cast<
        core::common::auto_expiry_concurrent_map::mock::
            MockAutoExpiryConcurrentMap<
                TimeGroup, std::shared_ptr<BudgetKeyTimeframeGroup>>*>(
        budget_key_timeframe_groups_.get());
  }

  std::function<core::ExecutionResult(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&,
      std::shared_ptr<BudgetKeyTimeframeGroup>&)>
      load_timeframe_group_from_db_mock;

  std::function<void(core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                                        LoadBudgetKeyTimeframeResponse>&,
                     std::shared_ptr<BudgetKeyTimeframeGroup>&,
                     core::AsyncContext<core::GetDatabaseItemRequest,
                                        core::GetDatabaseItemResponse>&)>
      on_load_timeframe_group_from_db_mock_callback_mock;
};
}  // namespace google::scp::pbs::buget_key_timeframe_manager::mock
