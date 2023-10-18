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
#include <utility>

#include "core/interface/transaction_manager_interface.h"

namespace google::scp::pbs {
class ConsumeBudgetCommandBase : public core::TransactionCommand {
 public:
  ConsumeBudgetCommandBase(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider,
      std::unique_ptr<core::common::OperationDispatcher> operation_dispatcher)
      : transaction_id_(transaction_id),
        budget_key_provider_(budget_key_provider),
        operation_dispatcher_(std::move(operation_dispatcher)) {}

  explicit ConsumeBudgetCommandBase(const core::common::Uuid& transaction_id)
      : transaction_id_(transaction_id) {}

  /**
   * @brief Get the Version of the command.
   *
   * @return core::Version
   */
  core::Version GetVersion() const { return version_; }

  /**
   * @brief Get the Transaction Id associated with this command.
   *
   * @return const core::common::Uuid
   */
  core::common::Uuid GetTransactionId() const { return transaction_id_; }

  /**
   * @brief Set up Command Execution Dependencies
   *
   * @param budget_key_provider
   * @param async_executor
   */
  virtual void SetUpCommandExecutionDependencies(
      const std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          async_executor) noexcept = 0;

 protected:
  /// The transaction ID associated with the command.
  const core::common::Uuid transaction_id_;

  /// An instance of the budget key provider that can provide the budget that
  /// needs to be consumed by the command.
  std::shared_ptr<BudgetKeyProviderInterface> budget_key_provider_;

  /// Operation dispatcher to dispatch budget consumption request on
  /// budget_key_provider
  std::unique_ptr<core::common::OperationDispatcher> operation_dispatcher_;

  /// Command's version
  const core::Version version_ = {.major = 1, .minor = 0};
};
}  // namespace google::scp::pbs
