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
#include <list>
#include <memory>

#include "pbs/budget_key/src/budget_key.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

namespace google::scp::pbs::budget_key::mock {

class MockBudgetKey : public BudgetKey {
 public:
  MockBudgetKey(
      const std::shared_ptr<BudgetKeyName>& name, const core::common::Uuid& id,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::JournalServiceInterface>& journal_service,
      const std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : BudgetKey(name, id, async_executor, journal_service,
                  nosql_database_provider, metric_client, config_provider,
                  std::make_shared<cpio::MockAggregateMetric>()) {}

  std::function<void(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&,
      core::common::Uuid&,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&)>
      on_log_load_budget_key_callback;

  std::function<core::ExecutionResult(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&)>
      load_budget_key_mock;

  std::function<core::ExecutionResult(
      std::shared_ptr<std::list<core::CheckpointLog>>&)>
      checkpoint_mock;

  std::function<core::ExecutionResult()> stop_mock;

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    if (stop_mock) {
      return stop_mock();
    }
    return core::SuccessExecutionResult();
  }

  void OnLogLoadBudgetKeyCallback(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context,
      core::common::Uuid& budget_key_timeframe_manager_id,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept override {
    if (on_log_load_budget_key_callback) {
      on_log_load_budget_key_callback(load_budget_key_context,
                                      budget_key_timeframe_manager_id,
                                      journal_log_context);
      return;
    }

    BudgetKey::OnLogLoadBudgetKeyCallback(load_budget_key_context,
                                          budget_key_timeframe_manager_id,
                                          journal_log_context);
  }

  core::ExecutionResult OnJournalServiceRecoverCallback(
      const std::shared_ptr<core::BytesBuffer>& bytes_buffer,
      const core::common::Uuid& activity_id) noexcept override {
    return BudgetKey::OnJournalServiceRecoverCallback(bytes_buffer,
                                                      activity_id);
  }

  core::ExecutionResult LoadBudgetKey(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context) noexcept override {
    if (load_budget_key_mock) {
      return load_budget_key_mock(load_budget_key_context);
    }

    return BudgetKey::LoadBudgetKey(load_budget_key_context);
  }

  core::ExecutionResult SerializeBudgetKey(
      core::common::Uuid& budget_key_timeframe_manager_id,
      core::BytesBuffer& budget_key_log_bytes_buffer) noexcept {
    return BudgetKey::SerializeBudgetKey(budget_key_timeframe_manager_id,
                                         budget_key_log_bytes_buffer);
  }

  core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>& checkpoint_logs) noexcept
      override {
    if (checkpoint_mock) {
      return checkpoint_mock(checkpoint_logs);
    }

    return BudgetKey::Checkpoint(checkpoint_logs);
  }

  core::common::Uuid GetBudgetKeyTimeframeManagerId() const noexcept {
    return budget_key_timeframe_manager_->GetId();
  }
};
}  // namespace google::scp::pbs::budget_key::mock
