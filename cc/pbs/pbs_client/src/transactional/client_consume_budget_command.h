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
#include <vector>

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/operation_dispatcher/src/retry_strategy.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/interface/pbs_client_interface.h"
#include "public/core/interface/execution_result.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kClientConsumeBudgetCommandRetryStrategyDelayMs = 31;
static constexpr size_t kClientConsumeBudgetCommandRetryStrategyTotalRetries =
    10;

namespace google::scp::pbs {
/*! @copydoc core::TransactionCommand
 */
class ClientConsumeBudgetCommand : public core::TransactionCommand {
 public:
  /**
   * @brief Constructs a new Client Consume Budget Command object.
   *
   * @param transaction_id The transaction id of the consume budget transaction.
   * @param transaction_secret The transaction secret of the consume budget
   * transaction.
   * @param budget_keys The budget keys in the transaction.
   * @param pbs_client The privacy budget service client.
   */
  ClientConsumeBudgetCommand(
      core::common::Uuid& transaction_id,
      std::shared_ptr<std::string>& transaction_secret,
      std::shared_ptr<std::vector<ConsumeBudgetMetadata>>& budget_keys,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<PrivacyBudgetServiceClientInterface>& pbs_client,
      const core::common::Uuid& parent_activity_id)
      : last_execution_timestamp_(UINT64_MAX),
        transaction_id_(transaction_id),
        transaction_secret_(transaction_secret),
        budget_keys_(budget_keys),
        pbs_client_(pbs_client),
        operation_dispatcher_(
            async_executor,
            core::common::RetryStrategy(
                core::common::RetryStrategyType::Exponential,
                kClientConsumeBudgetCommandRetryStrategyDelayMs,
                kClientConsumeBudgetCommandRetryStrategyTotalRetries)),
        parent_activity_id_(parent_activity_id) {
    begin = std::bind(&ClientConsumeBudgetCommand::Begin, this,
                      std::placeholders::_1);
    prepare = std::bind(&ClientConsumeBudgetCommand::Prepare, this,
                        std::placeholders::_1);
    commit = std::bind(&ClientConsumeBudgetCommand::Commit, this,
                       std::placeholders::_1);
    notify = std::bind(&ClientConsumeBudgetCommand::Notify, this,
                       std::placeholders::_1);
    abort = std::bind(&ClientConsumeBudgetCommand::Abort, this,
                      std::placeholders::_1);
    end = std::bind(&ClientConsumeBudgetCommand::End, this,
                    std::placeholders::_1);
    command_id = core::common::Uuid::GenerateUuid();
  }

 protected:
  /**
   * @brief Executes the prepare phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Begin(
      core::TransactionCommandCallback& callback) noexcept;

  /**
   * @brief Is called when the initiate consume budget transaction operation is
   * completed.
   *
   * @param consume_budget_transaction_context The consume budget transaction
   * context of the operation.
   * @param callback The callback on the completion of the operation.
   */
  virtual void OnInitiateConsumeBudgetTransactionCallback(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context,
      core::TransactionCommandCallback& callback) noexcept;

  /**
   * @brief Executes the prepare phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Prepare(
      core::TransactionCommandCallback& callback) noexcept;

  /**
   * @brief Executes the commit phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Commit(
      core::TransactionCommandCallback& callback) noexcept;

  /**
   * @brief Executes the notify phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Notify(
      core::TransactionCommandCallback& callback) noexcept;

  /**
   * @brief Executes the abort phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Abort(
      core::TransactionCommandCallback& callback) noexcept;

  /**
   * @brief Executes the prepare phase of a two-phase commit operation for
   * consuming budgets.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult End(
      core::TransactionCommandCallback& callback) noexcept;

  /**
   * @brief Executes a phase of the transaction. tracking of the phase is the
   responsibility of the transaction manager.
   *
   * @param transaction_execution_phase The transaction execution phase context
   of the operation.
   * @param transaction_phase_callback The transaction phase callback function.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult ExecuteTransactionPhase(
      core::TransactionExecutionPhase transaction_execution_phase,
      core::TransactionCommandCallback& transaction_phase_callback) noexcept;

  /**
   * @brief Is called when a phase of the transaction is executed.
   *
   * @param transaction_execution_phase The transaction execution phase context
   of the operation.
   * @param transaction_phase_callback The transaction phase callback function.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual void OnPhaseExecutionCallback(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context,
      core::TransactionCommandCallback& transaction_phase_callback) noexcept;

  /**
   * @brief Is executed when the last execution timestamp of the service does
   * not match with the client.
   *
   * @param transaction_phase_context The transaction phase context of the
   * operation.
   * @param get_transaction_status_context The get transaction status context.
   * @param transaction_phase_callback The transaction phase callback function.
   */
  virtual void OnExecuteTransactionPhaseGetStatusCallback(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context,
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context,
      core::TransactionCommandCallback& transaction_phase_callback) noexcept;

  /// The last execution timestamp of the transaction. At each phase, this value
  /// will change to guarantee optimistic concurrency of each operation.
  core::Timestamp last_execution_timestamp_;

 private:
  /// The transaction id.
  core::common::Uuid transaction_id_;
  /// The transaction secret.
  std::shared_ptr<std::string> transaction_secret_;
  /// The budget keys in the transactions.
  std::shared_ptr<std::vector<ConsumeBudgetMetadata>> budget_keys_;
  /// An instance of the privacy budget client to be used for executing the
  /// transactions.
  std::shared_ptr<PrivacyBudgetServiceClientInterface> pbs_client_;
  /// Operation dispatcher
  core::common::OperationDispatcher operation_dispatcher_;
  /// The parent activity id.
  core::common::Uuid parent_activity_id_;
};

}  // namespace google::scp::pbs
