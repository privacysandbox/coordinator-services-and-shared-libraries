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
#include <optional>
#include <utility>

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/transactions/src/consume_budget_command_common.h"
#include "public/core/interface/execution_result.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kConsumeBudgetCommandRetryStrategyDelayMs = 31;
static constexpr size_t kConsumeBudgetCommandRetryStrategyTotalRetries = 12;
static constexpr google::scp::core::common::Uuid kConsumeBudgetCommandId{
    1000ULL, 1000ULL};

namespace google::scp::pbs {
/**
 * @brief Implements consume budget command that uses two phase commit protocol.
 */
class ConsumeBudgetCommand : public core::TransactionCommand {
 public:
  ConsumeBudgetCommand(
      core::common::Uuid transaction_id,
      std::shared_ptr<BudgetKeyName>& budget_key_name,
      ConsumeBudgetCommandRequestInfo&& budget_consumption,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider)
      : transaction_id_(transaction_id),
        budget_key_name_(budget_key_name),
        budget_consumption_(std::move(budget_consumption)),
        async_executor_(async_executor),
        budget_key_provider_(budget_key_provider),
        operation_dispatcher_(
            async_executor,
            core::common::RetryStrategy(
                core::common::RetryStrategyType::Exponential,
                kConsumeBudgetCommandRetryStrategyDelayMs,
                kConsumeBudgetCommandRetryStrategyTotalRetries)),
        failed_with_insufficient_budget_consumption_(false) {
    begin = [](core::TransactionCommandCallback& callback) mutable {
      auto success_result = core::SuccessExecutionResult();
      callback(success_result);
      return core::SuccessExecutionResult();
    };
    prepare =
        std::bind(&ConsumeBudgetCommand::Prepare, this, std::placeholders::_1);
    commit =
        std::bind(&ConsumeBudgetCommand::Commit, this, std::placeholders::_1);
    notify =
        std::bind(&ConsumeBudgetCommand::Notify, this, std::placeholders::_1);
    abort =
        std::bind(&ConsumeBudgetCommand::Abort, this, std::placeholders::_1);
    end = begin;
    command_id = kConsumeBudgetCommandId;
  }

  ConsumeBudgetCommand(
      core::common::Uuid transaction_id,
      std::shared_ptr<BudgetKeyName>& budget_key_name, TimeBucket time_bucket,
      TokenCount total_budgets_to_consume,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider)
      : ConsumeBudgetCommand(transaction_id, budget_key_name,
                             ConsumeBudgetCommandRequestInfo(
                                 time_bucket, total_budgets_to_consume),
                             async_executor, budget_key_provider) {}

  /// Returns the budget key name associated with the command.
  const std::shared_ptr<BudgetKeyName> GetBudgetKeyName() const {
    return budget_key_name_;
  }

  /// Returns the time bucket associated with the command.
  TimeBucket GetTimeBucket() const { return budget_consumption_.time_bucket; }

  /// Returns the token count associated with the command.
  TokenCount GetTokenCount() const { return budget_consumption_.token_count; }

  /// Returns the budget consumption info
  ConsumeBudgetCommandRequestInfo GetBudgetConsumption() const {
    return budget_consumption_;
  }

  std::optional<size_t> GetRequestIndex() const {
    return budget_consumption_.request_index;
  }

  std::optional<ConsumeBudgetCommandRequestInfo>
  GetFailedInsufficientBudgetConsumption() const {
    if (failed_with_insufficient_budget_consumption_) {
      return budget_consumption_;
    }
    return std::nullopt;
  }

  /// Returns the current version of the command
  core::Version GetVersion() const { return {.major = 1, .minor = 0}; }

  /// Returns the transaction of the command
  const core::common::Uuid GetTransactionId() const { return transaction_id_; }

 protected:
  /**
   * @brief Executes the prepare phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Prepare(
      core::TransactionCommandCallback&) noexcept;

  /**
   * @brief Executes the commit phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Commit(
      core::TransactionCommandCallback&) noexcept;

  /**
   * @brief Executes the notify phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Notify(
      core::TransactionCommandCallback&) noexcept;

  /**
   * @brief Executes the abort phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Abort(
      core::TransactionCommandCallback&) noexcept;

  /**
   * @brief Is called once the budget key provider returns after loading the
   * budget key for the prepare phase.
   *
   * @param get_budget_key_context The get budget key context of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnPrepareGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Is called once the execution of prepare phase on the budget key is
   * completed.
   *
   * @param prepare_consume_budget_context The prepare consume budget context of
   * the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnPrepareConsumeBudgetCallback(
      core::AsyncContext<PrepareConsumeBudgetRequest,
                         PrepareConsumeBudgetResponse>&
          prepare_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Is called once the budget key provider returns after loading the
   * budget key for the commit phase.
   *
   * @param get_budget_key_context The get budget key context of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnCommitGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Is called once the execution of commit phase on the budget key is
   * completed.
   *
   * @param commit_consume_budget_context The commit consume budget context of
   * the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnCommitConsumeBudgetCallback(
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
          commit_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Is called once the budget key provider returns after loading the
   * budget key for the notify phase.
   *
   * @param get_budget_key_context The get budget key context of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnNotifyGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Is called once the execution of notify phase on the budget key is
   * completed.
   *
   * @param notify_consume_budget_context The notify consume budget context of
   * the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnNotifyConsumeBudgetCallback(
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
          notify_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Is called once the budget key provider returns after loading the
   * budget key for the abort phase.
   *
   * @param get_budget_key_context The get budget key context of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnAbortGetBudgetKeyCallback(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Is called once the execution of abort phase on the budget key is
   * completed.
   *
   * @param abort_consume_budget_context The abort consume budget context of
   * the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnAbortConsumeBudgetCallback(
      core::AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
          abort_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /// The transaction id associated with the command.
  core::common::Uuid transaction_id_;

  /// The budget key name for the current command.
  std::shared_ptr<BudgetKeyName> budget_key_name_;

  /// Budget consumption info
  ConsumeBudgetCommandRequestInfo budget_consumption_;

  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the budget key provider.
  std::shared_ptr<BudgetKeyProviderInterface> budget_key_provider_;

  /// Operation distpatcher
  core::common::OperationDispatcher operation_dispatcher_;

  /// If the command failed to execute any of its phases due to insufficient
  /// budget consumption
  bool failed_with_insufficient_budget_consumption_;
};

}  // namespace google::scp::pbs
