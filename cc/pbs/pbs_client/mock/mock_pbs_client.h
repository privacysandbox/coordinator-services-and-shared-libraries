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

#include <functional>

#include "pbs/interface/pbs_client_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::client::mock {
class MockPrivacyBudgetServiceClient
    : public PrivacyBudgetServiceClientInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult InitiateConsumeBudgetTransaction(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept override {
    if (initiate_consume_budget_transaction_mock) {
      return initiate_consume_budget_transaction_mock(
          consume_budget_transaction_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult ExecuteTransactionPhase(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context) noexcept override {
    if (execute_transaction_phase_mock) {
      return execute_transaction_phase_mock(transaction_phase_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override {
    if (get_transaction_status_mock) {
      return get_transaction_status_mock(get_transaction_status_context);
    }

    return core::SuccessExecutionResult();
  }

  std::function<core::ExecutionResult(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&)>
      initiate_consume_budget_transaction_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&)>
      execute_transaction_phase_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&)>
      get_transaction_status_mock;
};
}  // namespace google::scp::pbs::client::mock
