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
#include <memory>
#include <string>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "pbs/pbs_client/src/transactional/client_consume_budget_command.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::client::mock {
class MockClientConsumeBudgetCommand : public ClientConsumeBudgetCommand {
 public:
  MockClientConsumeBudgetCommand(
      core::common::Uuid transaction_id,
      std::shared_ptr<std::string>& transaction_secret,
      std::shared_ptr<std::vector<ConsumeBudgetMetadata>>& budget_keys,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<PrivacyBudgetServiceClientInterface>& pbs_client)
      : ClientConsumeBudgetCommand(transaction_id, transaction_secret,
                                   budget_keys, async_executor, pbs_client,
                                   core::common::kZeroUuid) {}

  virtual core::ExecutionResult Begin(
      core::TransactionCommandCallback& callback) noexcept {
    return ClientConsumeBudgetCommand::Begin(callback);
  }

  virtual void OnInitiateConsumeBudgetTransactionCallback(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context,
      core::TransactionCommandCallback& callback) noexcept {
    ClientConsumeBudgetCommand::OnInitiateConsumeBudgetTransactionCallback(
        consume_budget_transaction_context, callback);
  }

  virtual core::ExecutionResult Prepare(
      core::TransactionCommandCallback& callback) noexcept {
    return ClientConsumeBudgetCommand::Prepare(callback);
  }

  virtual core::ExecutionResult Commit(
      core::TransactionCommandCallback& callback) noexcept {
    return ClientConsumeBudgetCommand::Commit(callback);
  }

  virtual core::ExecutionResult Notify(
      core::TransactionCommandCallback& callback) noexcept {
    return ClientConsumeBudgetCommand::Notify(callback);
  }

  virtual core::ExecutionResult Abort(
      core::TransactionCommandCallback& callback) noexcept {
    return ClientConsumeBudgetCommand::Abort(callback);
  }

  virtual core::ExecutionResult End(
      core::TransactionCommandCallback& callback) noexcept {
    return ClientConsumeBudgetCommand::End(callback);
  }

  virtual core::ExecutionResult ExecuteTransactionPhase(
      core::TransactionExecutionPhase transaction_execution_phase,
      core::TransactionCommandCallback& transaction_phase_callback) noexcept {
    if (execute_transaction_phase_mock) {
      return execute_transaction_phase_mock(transaction_execution_phase,
                                            transaction_phase_callback);
    }
    return ClientConsumeBudgetCommand::ExecuteTransactionPhase(
        transaction_execution_phase, transaction_phase_callback);
  }

  virtual void OnPhaseExecutionCallback(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context,
      core::TransactionCommandCallback& transaction_phase_callback) noexcept {
    ClientConsumeBudgetCommand::OnPhaseExecutionCallback(
        transaction_phase_context, transaction_phase_callback);
  }

  core::Timestamp& GetLastExecutionTimestamp() {
    return last_execution_timestamp_;
  }

  std::function<core::ExecutionResult(core::TransactionExecutionPhase,
                                      core::TransactionCommandCallback&)>
      execute_transaction_phase_mock;
};
}  // namespace google::scp::pbs::client::mock
