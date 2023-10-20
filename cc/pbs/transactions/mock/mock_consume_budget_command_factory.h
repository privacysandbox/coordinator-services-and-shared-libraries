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

#include "pbs/transactions/mock/mock_batch_consume_budget_command.h"
#include "pbs/transactions/mock/mock_consume_budget_command.h"
#include "pbs/transactions/src/consume_budget_command_factory_interface.h"

namespace google::scp::pbs::transactions::mock {
class MockConsumeBudgetCommandFactory
    : public ConsumeBudgetCommandFactoryInterface {
 public:
  std::shared_ptr<core::TransactionCommand> ConstructCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<std::string>& budget_key_name,
      const ConsumeBudgetCommandRequestInfo& budget_info) noexcept override {
    return std::make_shared<MockConsumeBudgetCommand>(
        transaction_id, budget_key_name, budget_info, async_executor_,
        budget_key_provider_);
  }

  std::shared_ptr<core::TransactionCommand> ConstructBatchCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<std::string>& budget_key_name,
      const std::vector<ConsumeBudgetCommandRequestInfo>& budget_info) noexcept
      override {
    return std::make_shared<MockBatchConsumeBudgetCommand>(
        transaction_id, budget_key_name, budget_info, async_executor_,
        budget_key_provider_);
  }

  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  std::shared_ptr<BudgetKeyProviderInterface> budget_key_provider_;
};
}  // namespace google::scp::pbs::transactions::mock
