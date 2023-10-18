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

#include "core/interface/transaction_manager_interface.h"
#include "pbs/transactions/src/consume_budget_command_request_info.h"

namespace google::scp::pbs {
class ConsumeBudgetCommandFactoryInterface {
 public:
  virtual ~ConsumeBudgetCommandFactoryInterface() = default;

  /**
   * @brief Construct a consume budget command.
   *
   * @param transaction_id
   * @param budget_key_name
   * @param budget_info
   * @return std::shared_ptr<core::TransactionCommand>
   */
  virtual std::shared_ptr<core::TransactionCommand> ConstructCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<std::string>& budget_key_name,
      const ConsumeBudgetCommandRequestInfo& budget_info) noexcept = 0;

  /**
   * @brief Construct a batch consume budget command.
   *
   * @param transaction_id
   * @param budget_key_name
   * @param budget_info
   * @return std::shared_ptr<core::TransactionCommand>
   */
  virtual std::shared_ptr<core::TransactionCommand> ConstructBatchCommand(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<std::string>& budget_key_name,
      const std::vector<ConsumeBudgetCommandRequestInfo>&
          budget_info) noexcept = 0;
};
}  // namespace google::scp::pbs
