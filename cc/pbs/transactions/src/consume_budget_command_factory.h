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

#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_executor_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/transactions/src/consume_budget_command_factory_interface.h"

/**
 * @brief Implementation of the consume budget command factory interface
 *
 * @copydoc ConsumeBudgetCommandFactoryInterface
 */
namespace google::scp::pbs {
class ConsumeBudgetCommandFactory
    : public ConsumeBudgetCommandFactoryInterface {
 public:
  ConsumeBudgetCommandFactory(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider);

  std::shared_ptr<core::TransactionCommand> ConstructCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<std::string>& budget_key_name,
      const ConsumeBudgetCommandRequestInfo& budget_info) noexcept override;

  std::shared_ptr<core::TransactionCommand> ConstructBatchCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<std::string>& budget_key_name,
      const std::vector<ConsumeBudgetCommandRequestInfo>& budget_info) noexcept
      override;

 protected:
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  std::shared_ptr<BudgetKeyProviderInterface> budget_key_provider_;
};
}  // namespace google::scp::pbs
