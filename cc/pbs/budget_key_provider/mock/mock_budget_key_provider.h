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

#include "core/common/auto_expiry_concurrent_map/mock/mock_auto_expiry_concurrent_map.h"
#include "core/interface/async_executor_interface.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::budget_key_provider::mock {
class MockBudgetKeyProvider : public BudgetKeyProvider {
 public:
  MockBudgetKeyProvider(
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<core::JournalServiceInterface>& journal_service,
      std::shared_ptr<core::NoSQLDatabaseProviderInterface>&
          nosql_database_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : BudgetKeyProvider(async_executor, journal_service,
                          nosql_database_provider, metric_client,
                          config_provider) {
    budget_keys_ = std::make_unique<core::common::AutoExpiryConcurrentMap<
        std::string, std::shared_ptr<BudgetKeyProviderPair>>>(
        100, true /* extend_entry_lifetime_on_access */,
        true /* block_entry_while_eviction */,
        std::bind(&MockBudgetKeyProvider::OnBeforeGarbageCollection, this,
                  std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3),
        async_executor);
  }

  std::function<core::ExecutionResult(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&)>
      get_budget_key_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&,
      std::shared_ptr<BudgetKeyProviderPair>&)>
      log_load_budget_key_into_cache_mock;

  std::function<core::ExecutionResult(
      const std::shared_ptr<core::BytesBuffer>& bytes_buffer)>
      on_journal_service_recover_callback_mock;

  std::function<void(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context)>
      on_log_budget_key_into_cache_callback_mock;

  std::function<void(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&,
      std::shared_ptr<BudgetKeyProviderPair>&,
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&)>
      on_load_budget_key_callback_mock;

  std::function<void(std::string&, std::shared_ptr<BudgetKeyProviderPair>&,
                     std::function<void(bool)>)>
      on_before_garbage_collection_mock;

  std::function<void(
      std::function<void(bool)>,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&)>
      on_remove_entry_from_cache_logged_mock;

  core::ExecutionResult GetBudgetKey(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context) noexcept override {
    if (get_budget_key_mock) {
      return get_budget_key_mock(get_budget_key_context);
    }

    return BudgetKeyProvider::GetBudgetKey(get_budget_key_context);
  }

  core::ExecutionResult LogLoadBudgetKeyIntoCache(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair) noexcept
      override {
    if (log_load_budget_key_into_cache_mock) {
      return log_load_budget_key_into_cache_mock(get_budget_key_context,
                                                 budget_key_provider_pair);
    }

    return BudgetKeyProvider::LogLoadBudgetKeyIntoCache(
        get_budget_key_context, budget_key_provider_pair);
  }

  virtual core::ExecutionResult OnJournalServiceRecoverCallback(
      const std::shared_ptr<core::BytesBuffer>& bytes_buffer,
      const core::common::Uuid& activity_id) noexcept {
    if (on_journal_service_recover_callback_mock) {
      return on_journal_service_recover_callback_mock(bytes_buffer);
    }

    return BudgetKeyProvider::OnJournalServiceRecoverCallback(bytes_buffer,
                                                              activity_id);
  }

  virtual void OnLogLoadBudgetKeyIntoCacheCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept {
    if (on_log_budget_key_into_cache_callback_mock) {
      on_log_budget_key_into_cache_callback_mock(get_budget_key_context,
                                                 budget_key_provider_pair,
                                                 journal_log_context);
      return;
    }

    BudgetKeyProvider::OnLogLoadBudgetKeyIntoCacheCallback(
        get_budget_key_context, budget_key_provider_pair, journal_log_context);
  }

  virtual void OnLoadBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context) noexcept {
    if (on_load_budget_key_callback_mock) {
      on_load_budget_key_callback_mock(get_budget_key_context,
                                       budget_key_provider_pair,
                                       load_budget_key_context);
      return;
    }

    BudgetKeyProvider::OnLoadBudgetKeyCallback(get_budget_key_context,
                                               budget_key_provider_pair,
                                               load_budget_key_context);
  }

  virtual void OnBeforeGarbageCollection(
      std::string& budget_key,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      std::function<void(bool)> should_delete_entry) noexcept {
    if (on_before_garbage_collection_mock) {
      on_before_garbage_collection_mock(budget_key, budget_key_provider_pair,
                                        should_delete_entry);
      return;
    }

    BudgetKeyProvider::OnBeforeGarbageCollection(
        budget_key, budget_key_provider_pair, should_delete_entry);
  }

  virtual void OnRemoveEntryFromCacheLogged(
      std::function<void(bool)> should_delete_entry,
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
          journal_log_context) noexcept {
    if (on_remove_entry_from_cache_logged_mock) {
      on_remove_entry_from_cache_logged_mock(should_delete_entry,
                                             journal_log_context);
      return;
    }

    BudgetKeyProvider::OnRemoveEntryFromCacheLogged(
        should_delete_entry, budget_key_provider_pair, journal_log_context);
  }

  virtual core::ExecutionResult SerializeBudgetKeyProviderPair(
      std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
      budget_key_provider::proto::OperationType operation_type,
      core::BytesBuffer& budget_key_provider_log_bytes_buffer) noexcept {
    return BudgetKeyProvider::SerializeBudgetKeyProviderPair(
        budget_key_provider_pair, operation_type,
        budget_key_provider_log_bytes_buffer);
  }

  auto& GetBudgetKeys() { return budget_keys_; }

  auto* GetInternalBudgetKeys() {
    return static_cast<
        core::common::auto_expiry_concurrent_map::mock::
            MockAutoExpiryConcurrentMap<
                std::string, std::shared_ptr<BudgetKeyProviderPair>>*>(
        budget_keys_.get());
  }
};
}  // namespace google::scp::pbs::budget_key_provider::mock
