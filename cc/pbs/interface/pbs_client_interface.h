// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/interface/front_end_service_interface.h"

namespace google::scp::pbs {
/**
 * @brief Provides privacy budget service API layer for a single privacy budget
 * instance.
 */
class PrivacyBudgetServiceClientInterface : public core::ServiceInterface {
 public:
  virtual ~PrivacyBudgetServiceClientInterface() = default;

  /**
   * @brief Inquires the transaction status from the remote transaction engine.
   *
   * @param get_transaction_status_context The get transaction status context.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept = 0;

  /**
   * @brief Initiates a new consume budget transaction on a privacy budget
   * service.
   *
   * @param consume_budget_transaction_context The consume budget transaction
   * context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult InitiateConsumeBudgetTransaction(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept = 0;

  /**
   * @brief Executes a transaction phase on a privacy budget service.
   *
   * @param transaction_phase_context The transaction phase context of the
   * operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult ExecuteTransactionPhase(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context) noexcept = 0;
};
}  // namespace google::scp::pbs
