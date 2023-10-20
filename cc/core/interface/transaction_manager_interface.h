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
#include <string>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/checkpoint_service_interface.h"

#include "async_context.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {

typedef std::function<void(ExecutionResult&)> TransactionCommandCallback;
typedef std::function<ExecutionResult(TransactionCommandCallback&)>
    TransactionAction;

/**
 * @brief The context of any transaction command. This context will be shared
 * between all the actions of a transaction to synchronize state.
 */
struct TransactionCommandContext {};

/**
 * @brief To keep the transaction phase actions. A two phase commit transaction
 * protocol requires four different actions, i.e., 1) prepare: the initial
 * checks that need to happen to evaluate the feasibility of the operation. 2)
 * commit: apply the changes in the pending state but not finalize. 3) abort:
 * cancel the operation and clean up the state. 4) notify: finalize the changes,
 * whether it is a commit operation or abort.
 */
struct TransactionCommand {
  virtual ~TransactionCommand() = default;

  /// Id of the transaction command.
  core::common::Uuid command_id = core::common::kZeroUuid;
  /// Begin phase handler.
  TransactionAction begin;
  /// Prepare phase handler.
  TransactionAction prepare;
  /// Commit phase handler.
  TransactionAction commit;
  /// Abort phase handler.
  TransactionAction abort;
  /// Notify phase handler.
  TransactionAction notify;
  /// End phase handler.
  TransactionAction end;
};

/**
 * @brief TransactionRequest includes all the commands need to the executed
 * by the transaction manager within a transaction.
 */
struct TransactionRequest {
  /// Id of the transaction.
  core::common::Uuid transaction_id = {};
  /// Vector of all the commands for a transaction.
  std::vector<std::shared_ptr<TransactionCommand>> commands;
  /// Timestamp of when the transaction expires.
  /// NOTE: This is unused, see Transaction Engine.cc for the transaction
  /// lifetime.
  Timestamp timeout_time;
  /// Indicates whether the transaction is coordinated by a remote transaction
  /// manager.
  bool is_coordinated_remotely = false;
  /// The secret of the transaction.
  std::shared_ptr<std::string> transaction_secret;
  /// The origin of the transaction aka domain name of the caller.
  std::shared_ptr<std::string> transaction_origin;
};

/**
 * @brief The response object from the transaction manager once the transaction
 * is finished.
 */
struct TransactionResponse {
  /// Id of the completed transaction.
  core::common::Uuid transaction_id = {};
  /// The last execution time stamp of any phases of a transaction. This will
  /// provide optimistic concurrency behavior for the transaction execution.
  Timestamp last_execution_timestamp = 0;
  /// List of all the indices that failed in the current phase of execution.
  std::list<size_t> failed_commands_indices;
  /// List of all the commands that failed in the current phase of execution.
  std::list<std::shared_ptr<TransactionCommand>> failed_commands;
};

/// Enum class representing all the transaction execution phases.
enum class TransactionExecutionPhase {
  Begin = 1,
  Prepare = 2,
  Commit = 3,
  Notify = 4,
  Abort = 5,
  End = 6,
  Unknown = 1000
};

/**
 * @brief Provides transaction phase execution capabilities for the remotely
 * coordinated transactions.
 */
struct TransactionPhaseRequest {
  /// Id of the transaction.
  core::common::Uuid transaction_id = {};
  /// The transaction phase to be executed.
  TransactionExecutionPhase transaction_execution_phase =
      TransactionExecutionPhase::Unknown;
  /// In the case of remote transaction, the transaction secret will provide
  /// other participants to inquiry or update the state of a transaction.
  std::shared_ptr<std::string> transaction_secret;
  /// The origin of the transaction aka domain name of the caller.
  std::shared_ptr<std::string> transaction_origin;
  /// The last execution time stamp of any phases of a transaction. This will
  /// provide optimistic concurrency behavior for the transaction execution.
  Timestamp last_execution_timestamp;
};

/**
 * @brief Includes the response metadata for the transaction phase execution of
 * remotely coordinated transactions.
 *
 */
struct TransactionPhaseResponse {
  /// The last execution time stamp of any phases of a transaction. This will
  /// provide optimistic concurrency behavior for the transaction execution.
  Timestamp last_execution_timestamp;
  /// List of all the indices that failed in the current phase of execution.
  std::list<size_t> failed_commands_indices;
  /// List of all the commands that failed in the current phase of execution.
  std::list<std::shared_ptr<TransactionCommand>> failed_commands;
};

/**
 * @brief Represents the get transaction status request object. This is used
 * when two participants of the transactions need to inquire the status of the
 * transaction.
 */
struct GetTransactionStatusRequest {
  /// Id of the transaction.
  core::common::Uuid transaction_id = {};
  /// In the case of remote transaction, the transaction secret will provide
  /// other participants to inquiry or update the state of a transaction.
  std::shared_ptr<std::string> transaction_secret;
  /// The origin of the transaction aka domain name of the caller.
  std::shared_ptr<std::string> transaction_origin;
};

/**
 * @brief Represents the get transaction status response object.
 */
struct GetTransactionStatusResponse {
  /// The phase of the current transaction.
  TransactionExecutionPhase transaction_execution_phase;
  /// The last execution time stamp of any phases of a transaction. This will
  /// provide optimistic concurrency behavior for the transaction execution.
  Timestamp last_execution_timestamp;
  /// Indicates whether a transaction is expired.
  bool is_expired = false;
  /// Indicates whether the transaction has failures on the current phase or the
  /// transaction has failed.
  bool has_failure = false;
};

/**
 * @brief Represents the Request object of GetStatus API to get details of
 * Transaction Manager (TM) current state
 */
struct GetTransactionManagerStatusRequest {};

/**
 * @brief Represents the Response object of GetStatus API to get details of
 * Transaction Manager (TM) current state
 */
struct GetTransactionManagerStatusResponse {
  size_t pending_transactions_count;
};

/**
 * @brief Transaction manager is responsible for running 2-phase commit
 * transactions in a form of transaction commands. To execute a transaction
 * one must create a transaction request, provide the transaction commands,
 * and call the execute function.
 */
class TransactionManagerInterface : public ServiceInterface {
 public:
  virtual ~TransactionManagerInterface() = default;

  /**
   * @brief To execute a transaction, the caller must provide a transaction
   * request object with a series of commands to be executed atomically.
   *
   * @param transactionContext the async context of the transaction.
   * @return ExecutionResult the result of the execution.
   */
  virtual ExecutionResult Execute(
      AsyncContext<TransactionRequest, TransactionResponse>&
          transaction_context) noexcept = 0;

  /**
   * @brief To execute a transaction phase, the caller must provide the
   * transaction id and the desired phase to be executed.
   *
   * @param transaction_phase_context The transaction phase context.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ExecutePhase(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_phase_context) noexcept = 0;

  /**
   * @brief Creates a checkpoint of the current transaction manager state.
   *
   * @param checkpoint_logs The vector of checkpoint logs.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Checkpoint(
      std::shared_ptr<std::list<CheckpointLog>>& checkpoint_logs) noexcept = 0;

  /**
   * @brief Inquires the transaction status.
   *
   * @param get_transaction_status_context The get transaction status context.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult GetTransactionStatus(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept = 0;

  /**
   * @brief Get the current status of Transaction Manager (TM)
   *
   * @param request Request object to query status
   * @param response Response object which contains details of current state of
   * the TM
   * @return ExecutionResult
   */
  virtual ExecutionResult GetTransactionManagerStatus(
      const GetTransactionManagerStatusRequest& request,
      GetTransactionManagerStatusResponse& response) noexcept = 0;
};
}  // namespace google::scp::core
