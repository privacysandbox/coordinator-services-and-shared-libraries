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
#include "pbs/transactions/src/consume_budget_command_base.h"
#include "pbs/transactions/src/consume_budget_command_request_info.h"
#include "public/core/interface/execution_result.h"

static constexpr google::scp::core::common::Uuid kConsumeBudgetCommandId{
    1000ULL, 1000ULL};

namespace google::scp::pbs {
/**
 * @brief Implements consume budget command that uses two phase commit protocol.
 */
class ConsumeBudgetCommand : public ConsumeBudgetCommandBase {
 public:
  /**
   * @brief Construct a new Consume Budget Command object with execution
   * dependencies
   *
   * @param transaction_id
   * @param budget_key_name
   * @param budget_consumption
   * @param async_executor
   * @param budget_key_provider
   */
  ConsumeBudgetCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<BudgetKeyName>& budget_key_name,
      const ConsumeBudgetCommandRequestInfo& budget_consumption,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider);

  /**
   * @brief Construct a new Consume Budget Command object with deferred setting
   * of execution dependencies. The dependencies will be set by the component
   * handling the execution of the command.
   *
   * @param transaction_id
   * @param budget_key_name
   * @param budget_consumption
   */
  ConsumeBudgetCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<BudgetKeyName>& budget_key_name,
      const ConsumeBudgetCommandRequestInfo& budget_consumption);

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

  /// Set the dependencies provided.
  void SetUpCommandExecutionDependencies(
      const std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          async_executor) noexcept override;

 protected:
  /**
   * @brief Set up handlers for phases such as BEGIN, PREPARE, COMMIT, etc.
   */
  void SetUpCommandPhaseHandlers();

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

  /// The budget key name for the current command.
  const std::shared_ptr<BudgetKeyName> budget_key_name_;

  /// Budget consumption info
  const ConsumeBudgetCommandRequestInfo budget_consumption_;

  /// If the command failed to execute any of its phases due to insufficient
  /// budget consumption
  bool failed_with_insufficient_budget_consumption_;
};

}  // namespace google::scp::pbs
