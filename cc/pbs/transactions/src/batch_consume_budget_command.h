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

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/transactions/src/consume_budget_command_base.h"
#include "pbs/transactions/src/consume_budget_command_request_info.h"
#include "public/core/interface/execution_result.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kBatchConsumeBudgetCommandRetryStrategyDelayMs = 31;
static constexpr size_t kBatchConsumeBudgetCommandRetryStrategyTotalRetries =
    12;
// NOTE: Please refer to the existing command IDs to ensure there is no conflict
// when picking a new command ID
static constexpr google::scp::core::common::Uuid kBatchConsumeBudgetCommandId{
    1000ULL, 1001ULL};

namespace google::scp::pbs {
/**
 * @brief Implements batch consume budget command that uses two phase commit
 * protocol to commit a batch of budgets belonging to the same budget key.
 * NOTE: Batching is per budgetkey.
 */
class BatchConsumeBudgetCommand : public ConsumeBudgetCommandBase {
 public:
  /**
   * @brief Construct a new Consume Budget Command object with execution
   * dependencies
   *
   * @param transaction_id
   * @param budget_key_name
   * @param budget_consumptions
   * @param async_executor
   * @param budget_key_provider
   */
  BatchConsumeBudgetCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<BudgetKeyName>& budget_key_name,
      const std::vector<ConsumeBudgetCommandRequestInfo>& budget_consumptions,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider);

  /**
   * @brief Construct a new Consume Budget Command object with deferred setting
   * of execution dependencies. The dependencies will be set by the component
   * handling the execution of the command.
   *
   * @param transaction_id
   * @param budget_key_name
   * @param budget_consumptions
   */
  BatchConsumeBudgetCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<BudgetKeyName>& budget_key_name,
      const std::vector<ConsumeBudgetCommandRequestInfo>& budget_consumptions);

  /// Returns the budget key name associated with the command.
  const std::shared_ptr<BudgetKeyName> GetBudgetKeyName() const {
    return budget_key_name_;
  }

  const std::vector<ConsumeBudgetCommandRequestInfo>& GetBudgetConsumptions()
      const {
    return budget_consumptions_;
  }

  const std::vector<ConsumeBudgetCommandRequestInfo>&
  GetFailedInsufficientBudgetConsumptions() const {
    return failed_insufficient_budget_consumptions_;
  }

  /// Set up the dependencies provided.
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
   * @param prepare_batch_consume_budget_context The prepare consume budget
   * context of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnPrepareBatchConsumeBudgetCallback(
      core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                         PrepareBatchConsumeBudgetResponse>&
          prepare_batch_consume_budget_context,
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
   * @param commit_batch_consume_budget_context The commit consume budget
   * context of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnCommitBatchConsumeBudgetCallback(
      core::AsyncContext<CommitBatchConsumeBudgetRequest,
                         CommitBatchConsumeBudgetResponse>&
          commit_batch_consume_budget_context,
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
   * @param notify_batch_consume_budget_context The notify consume budget
   * context of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnNotifyBatchConsumeBudgetCallback(
      core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                         NotifyBatchConsumeBudgetResponse>&
          notify_batch_consume_budget_context,
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
   * @param abort_batch_consume_budget_context The abort consume budget context
   * of the operation.
   * @param transaction_command_callback The transaction callback function.
   */
  virtual void OnAbortBatchConsumeBudgetCallback(
      core::AsyncContext<AbortBatchConsumeBudgetRequest,
                         AbortBatchConsumeBudgetResponse>&
          abort_batch_consume_budget_context,
      core::TransactionCommandCallback& transaction_command_callback) noexcept;

  /**
   * @brief Copy over set of budgets from the budget_consumptions_ to
   * failed_insufficient_budget_consumptions_ based on Prepare/Commit request
   * response.
   *
   * This is meant to be used only for PrepareBatchConsumeBudgetRequest/Response
   * and CommitBatchConsumeBudgetRequest/Response
   *
   * @param budget_consumptions
   */
  template <class Request, class Response>
  void SetFailedInsufficientBudgetConsumptions(
      const core::AsyncContext<Request, Response>& context);

  /// The budget key name for the current command.
  std::shared_ptr<BudgetKeyName> budget_key_name_;

  /// The budget key consumptions info
  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions_;

  /// The budget key consumptions that failed during command phase execution due
  /// to insufficient budget
  std::vector<ConsumeBudgetCommandRequestInfo>
      failed_insufficient_budget_consumptions_;
};

}  // namespace google::scp::pbs
