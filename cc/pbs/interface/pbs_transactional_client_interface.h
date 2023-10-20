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

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

#include "front_end_service_interface.h"

namespace google::scp::pbs {
/**
 * @brief Provides transactional privacy budget service client functionality for
 * the consume budget operation.
 */
class PrivacyBudgetServiceTransactionalClientInterface
    : public core::ServiceInterface {
 public:
  virtual ~PrivacyBudgetServiceTransactionalClientInterface() = default;

  /**
   * @brief Executes consume budget operation against multiple instances of
   * privacy budget service.
   *
   * @param consume_budget_context The consume budget context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult ConsumeBudget(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_context) noexcept = 0;

  /**
   * @brief Get the Transaction Status On PBS 1 object
   *
   * @param get_transaction_status_context
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult GetTransactionStatusOnPBS1(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>
          get_transaction_status_context) noexcept = 0;

  /**
   * @brief Get the Transaction Status On PBS 2 object
   *
   * @param get_transaction_status_context
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult GetTransactionStatusOnPBS2(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>
          get_transaction_status_context) noexcept = 0;
};

}  // namespace google::scp::pbs
