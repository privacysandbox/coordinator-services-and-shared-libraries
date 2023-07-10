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
#include <string>

#include "core/interface/async_context.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/service_interface.h"

#include "budget_key_interface.h"
#include "type_def.h"

namespace google::scp::pbs {

/// Is used to lookup a specific budget key from the key space.
struct GetBudgetKeyRequest {
  std::shared_ptr<std::string> budget_key_name;
};

/// GetBudgetKeyResponse includes the budget_key pointer.
struct GetBudgetKeyResponse {
  std::shared_ptr<BudgetKeyInterface> budget_key;
};

/**
 * @brief BudgetKeyProvider loads all the keys and is able to provide a
 * reference to all the requested BudgetKeys for performing budgeting
 * operations.
 */
class BudgetKeyProviderInterface : public core::ServiceInterface {
 public:
  virtual ~BudgetKeyProviderInterface() = default;

  /**
   * @brief Returns the budget key with a specific key info.
   *
   * @param get_budget_key_context the async context of the GetBudgetKey
   * operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult GetBudgetKey(
      core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
          get_budget_key_context) noexcept = 0;

  /**
   * @brief Creates a checkpoint of the current transaction manager state.
   *
   * @param checkpoint_logs The vector of checkpoint logs.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>&
          checkpoint_logs) noexcept = 0;
};
}  // namespace google::scp::pbs
