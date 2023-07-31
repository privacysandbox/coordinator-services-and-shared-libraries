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

#include <list>
#include <memory>
#include <string_view>

#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::transaction_manager {
/**
 * @brief The transaction engine is responsible to execute the transactions and
 * from the start to the end.
 */
class TransactionEngineInterface : public ServiceInterface {
 public:
  ~TransactionEngineInterface() = default;

  /**
   * @brief Executes a transaction provided by the transaction context.
   *
   * @param transaction_context The context including transaction metadata
   * and callback.
   * @return ExecutionResult The result of the execute operation.
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
   * @brief Get the count of transactions that have not finished execution in
   * the transaction engine.
   *
   * @return size_t count
   */
  virtual size_t GetPendingTransactionCount() noexcept = 0;
};
}  // namespace google::scp::core::transaction_manager
