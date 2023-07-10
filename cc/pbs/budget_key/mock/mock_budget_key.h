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

#include "pbs/budget_key/src/budget_key.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::budget_key::mock {

class MockBudgetKey : public BudgetKeyInterface {
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

  core::ExecutionResult CanUnload() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult LoadBudgetKey(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context) noexcept override {
    load_budget_key_context.response =
        std::make_shared<LoadBudgetKeyResponse>();
    load_budget_key_context.result = core::SuccessExecutionResult();
    load_budget_key_context.Finish();
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult GetBudget(
      core::AsyncContext<GetBudgetRequest, GetBudgetResponse>&
          get_budget_context) noexcept override {
    get_budget_context.response = std::make_shared<GetBudgetResponse>();
    get_budget_context.response->token_count = token_count;
    get_budget_context.Finish();

    return core::SuccessExecutionResult();
  }

  const std::shared_ptr<ConsumeBudgetTransactionProtocolInterface>
  GetBudgetConsumptionTransactionProtocol() noexcept override {
    return budget_consumption_transaction_protocol;
  }

  const std::shared_ptr<BatchConsumeBudgetTransactionProtocolInterface>
  GetBatchBudgetConsumptionTransactionProtocol() noexcept override {
    return batch_budget_consumption_transaction_protocol;
  }

  const std::shared_ptr<BudgetKeyName> GetName() noexcept override {
    return name;
  }

  const core::common::Uuid GetId() noexcept override {
    return core::common::Uuid::GenerateUuid();
  }

  core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>&
          checkpoint_logs) noexcept {
    return core::SuccessExecutionResult();
  }

  std::shared_ptr<ConsumeBudgetTransactionProtocolInterface>
      budget_consumption_transaction_protocol;

  std::shared_ptr<BatchConsumeBudgetTransactionProtocolInterface>
      batch_budget_consumption_transaction_protocol;

  std::shared_ptr<BudgetKeyName> name =
      std::make_shared<BudgetKeyName>("Mock_Budget_Key");
  TokenCount token_count;
};
}  // namespace google::scp::pbs::budget_key::mock
