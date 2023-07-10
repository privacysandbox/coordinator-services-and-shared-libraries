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

#include "core/interface/transaction_manager_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core {
/**
 * @brief The remote transaction engine to provide remote transaction operations
 * on transactions.
 */
class RemoteTransactionManagerInterface : public ServiceInterface {
 public:
  virtual ~RemoteTransactionManagerInterface() = default;

  /**
   * @brief Inquires the transaction status from the remote transaction engine.
   *
   * @param get_transaction_status_context The get transaction status context.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult GetTransactionStatus(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept = 0;

  /**
   * @brief Executes the requested phase on the remote transaction engine.
   *
   * @param transaction_phase_context The get transaction phase context.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ExecutePhase(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_phase_context) noexcept = 0;
};
}  // namespace google::scp::core
