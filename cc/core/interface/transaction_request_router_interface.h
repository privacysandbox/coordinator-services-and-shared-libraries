

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

#include "core/interface/http_types.h"
#include "core/interface/service_interface.h"
#include "core/interface/transaction_manager_interface.h"

namespace google::scp::core {
/**
 * @brief Abstraction to route transaction requests without the caller being
 * aware of the target component that executes the transactions. Routes a
 * transaction request to a target component that can handle the request. The
 * target component could either be the single global Transaction Manager or a
 * Partition that accepts Transactions.
 */
class TransactionRequestRouterInterface {
 public:
  virtual ~TransactionRequestRouterInterface() = default;

  /**
   * @brief Execute TransactionRequest. This is the first request caller creates
   * to kick start a transaction.
   *
   * @param context
   * @return ExecutionResult
   */
  virtual ExecutionResult Execute(
      AsyncContext<TransactionRequest, TransactionResponse>&
          context) noexcept = 0;

  /**
   * @brief Execute TransactionPhaseRequest. This request type is created for
   * each phase the caller want to execute.
   *
   * @param context
   * @return ExecutionResult
   */
  virtual ExecutionResult Execute(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          context) noexcept = 0;

  /**
   * @brief Execute GetTransactionStatus request. Get status of a transaction
   * in the system.
   *
   * @param context
   * @return ExecutionResult
   */
  virtual ExecutionResult Execute(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          context) noexcept = 0;

  /**
   * @brief Execute GetTransactionManagerStatusRequest. Get aggregate status of
   * all transactions executing in the system.
   *
   * @param request
   * @param response
   * @return ExecutionResult
   */
  virtual ExecutionResult Execute(
      const GetTransactionManagerStatusRequest& request,
      GetTransactionManagerStatusResponse& response) noexcept = 0;
};
}  // namespace google::scp::core
