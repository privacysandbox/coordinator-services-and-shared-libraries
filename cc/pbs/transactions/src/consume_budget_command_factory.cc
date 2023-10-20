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

#include "pbs/transactions/src/consume_budget_command_factory.h"

#include "pbs/transactions/src/batch_consume_budget_command.h"
#include "pbs/transactions/src/consume_budget_command.h"

namespace google::scp::pbs {

ConsumeBudgetCommandFactory::ConsumeBudgetCommandFactory(
    const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
    const std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider)
    : async_executor_(async_executor),
      budget_key_provider_(budget_key_provider) {}

std::shared_ptr<core::TransactionCommand>
ConsumeBudgetCommandFactory::ConstructCommand(
    const core::common::Uuid& transaction_id,
    const std::shared_ptr<std::string>& budget_key_name,
    const ConsumeBudgetCommandRequestInfo& budget_info) noexcept {
  return std::make_shared<ConsumeBudgetCommand>(transaction_id, budget_key_name,
                                                budget_info, async_executor_,
                                                budget_key_provider_);
}

std::shared_ptr<core::TransactionCommand>
ConsumeBudgetCommandFactory::ConstructBatchCommand(
    const core::common::Uuid& transaction_id,
    const std::shared_ptr<std::string>& budget_key_name,
    const std::vector<ConsumeBudgetCommandRequestInfo>& budget_info) noexcept {
  return std::make_shared<BatchConsumeBudgetCommand>(
      transaction_id, budget_key_name, budget_info, async_executor_,
      budget_key_provider_);
}
}  // namespace google::scp::pbs
