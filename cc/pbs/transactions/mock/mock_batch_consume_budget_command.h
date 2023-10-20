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
#include <utility>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/transactions/src/batch_consume_budget_command.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::transactions::mock {

class MockBatchConsumeBudgetCommand : public BatchConsumeBudgetCommand {
 public:
  MockBatchConsumeBudgetCommand(
      const core::common::Uuid transaction_id,
      const std::shared_ptr<BudgetKeyName>& budget_key_name,
      const std::vector<ConsumeBudgetCommandRequestInfo>& budget_consumptions,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider)
      : BatchConsumeBudgetCommand(transaction_id, budget_key_name,
                                  move(budget_consumptions), async_executor,
                                  budget_key_provider) {}

  std::function<core::ExecutionResult(core::TransactionCommandCallback&)>
      prepare_mock;

  std::function<core::ExecutionResult(core::TransactionCommandCallback&)>
      commit_mock;

  std::function<core::ExecutionResult(core::TransactionCommandCallback&)>
      notify_mock;

  std::function<core::ExecutionResult(core::TransactionCommandCallback&)>
      abort_mock;

  std::function<void(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&,
      core::TransactionCommandCallback&)>
      on_prepare_get_budget_key_callback_mock;

  std::function<void(core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                                        PrepareBatchConsumeBudgetResponse>&,
                     core::TransactionCommandCallback&)>
      on_prepare_consume_budget_callback_mock;

  std::function<void(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&,
      core::TransactionCommandCallback&)>
      on_commit_get_budget_key_callback_mock;

  std::function<void(core::AsyncContext<CommitBatchConsumeBudgetRequest,
                                        CommitBatchConsumeBudgetResponse>&,
                     core::TransactionCommandCallback&)>
      on_commit_consume_budget_callback_mock;

  std::function<void(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&,
      core::TransactionCommandCallback&)>
      on_notify_get_budget_key_callback_mock;

  std::function<void(core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                                        NotifyBatchConsumeBudgetResponse>&,
                     core::TransactionCommandCallback&)>
      on_notify_consume_budget_callback_mock;

  std::function<void(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&,
      core::TransactionCommandCallback&)>
      on_abort_get_budget_key_callback_mock;

  std::function<void(core::AsyncContext<AbortBatchConsumeBudgetRequest,
                                        AbortBatchConsumeBudgetResponse>&,
                     core::TransactionCommandCallback&)>
      on_abort_consume_budget_callback_mock;

  core::ExecutionResult Prepare(
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (prepare_mock) {
      return prepare_mock(transaction_command_callback);
    }

    return BatchConsumeBudgetCommand::Prepare(transaction_command_callback);
  }

  core::ExecutionResult Commit(
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (commit_mock) {
      return commit_mock(transaction_command_callback);
    }

    return BatchConsumeBudgetCommand::Prepare(transaction_command_callback);
  }

  core::ExecutionResult Notify(
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (notify_mock) {
      return notify_mock(transaction_command_callback);
    }

    return BatchConsumeBudgetCommand::Prepare(transaction_command_callback);
  }

  core::ExecutionResult Abort(
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (abort_mock) {
      return abort_mock(transaction_command_callback);
    }

    return BatchConsumeBudgetCommand::Prepare(transaction_command_callback);
  }

  void OnPrepareGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_prepare_get_budget_key_callback_mock) {
      on_prepare_get_budget_key_callback_mock(get_budget_key_context,
                                              transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnPrepareGetBudgetKeyCallback(
        get_budget_key_context, transaction_command_callback);
  }

  void OnPrepareBatchConsumeBudgetCallback(
      core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                         PrepareBatchConsumeBudgetResponse>&
          prepare_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_prepare_consume_budget_callback_mock) {
      on_prepare_consume_budget_callback_mock(prepare_consume_budget_context,
                                              transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnPrepareBatchConsumeBudgetCallback(
        prepare_consume_budget_context, transaction_command_callback);
  }

  void OnCommitGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_commit_get_budget_key_callback_mock) {
      on_commit_get_budget_key_callback_mock(get_budget_key_context,
                                             transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnCommitGetBudgetKeyCallback(
        get_budget_key_context, transaction_command_callback);
  }

  void OnCommitBatchConsumeBudgetCallback(
      core::AsyncContext<CommitBatchConsumeBudgetRequest,
                         CommitBatchConsumeBudgetResponse>&
          commit_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_commit_consume_budget_callback_mock) {
      on_commit_consume_budget_callback_mock(commit_consume_budget_context,
                                             transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnCommitBatchConsumeBudgetCallback(
        commit_consume_budget_context, transaction_command_callback);
  }

  void OnNotifyGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_notify_get_budget_key_callback_mock) {
      on_notify_get_budget_key_callback_mock(get_budget_key_context,
                                             transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnNotifyGetBudgetKeyCallback(
        get_budget_key_context, transaction_command_callback);
  }

  void OnNotifyBatchConsumeBudgetCallback(
      core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                         NotifyBatchConsumeBudgetResponse>&
          notify_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_notify_consume_budget_callback_mock) {
      on_notify_consume_budget_callback_mock(notify_consume_budget_context,
                                             transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnNotifyBatchConsumeBudgetCallback(
        notify_consume_budget_context, transaction_command_callback);
  }

  void OnAbortGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_abort_get_budget_key_callback_mock) {
      on_abort_get_budget_key_callback_mock(get_budget_key_context,
                                            transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnAbortGetBudgetKeyCallback(
        get_budget_key_context, transaction_command_callback);
  }

  void OnAbortBatchConsumeBudgetCallback(
      core::AsyncContext<AbortBatchConsumeBudgetRequest,
                         AbortBatchConsumeBudgetResponse>&
          abort_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept
      override {
    if (on_abort_consume_budget_callback_mock) {
      on_abort_consume_budget_callback_mock(abort_consume_budget_context,
                                            transaction_command_callback);
      return;
    }

    BatchConsumeBudgetCommand::OnAbortBatchConsumeBudgetCallback(
        abort_consume_budget_context, transaction_command_callback);
  }
};

}  // namespace google::scp::pbs::transactions::mock
