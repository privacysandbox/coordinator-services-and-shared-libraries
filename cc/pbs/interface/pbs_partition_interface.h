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

#include "core/interface/async_context.h"
#include "core/interface/partition_interface.h"
#include "core/interface/transaction_manager_interface.h"

namespace google::scp::pbs {

/**
 * @brief All requests to this are forwarded to Transaction Manager for
 * execution.
 *
 * Unloading a Partition Stops all of the components that the partition manages
 * For instance, In PBS case, the components will be Transaction Manager, Budget
 * Key Provider, Budget Key Timeframe Manager, Checkpoint Service, etc. and also
 * cancels any pending work related to this partition on AsyncExecutors.
 *
 * NOTE: The interface of this mimics TransactionManagerInterface.
 *
 */
class PBSPartitionInterface : public core::PartitionInterface {
 public:
  /**
   * @brief Forwards request to Transaction Manager for execution. See
   * TransactionManagerInterface.
   *
   * @param context
   * @return ExecutionResult
   */
  virtual core::ExecutionResult ExecuteRequest(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>& context) noexcept = 0;

  /**
   * @brief Forwards request to Transaction Manager for execution. See
   * TransactionManagerInterface.
   *
   * @param context
   * @return ExecutionResult
   */
  virtual core::ExecutionResult ExecuteRequest(
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          context) noexcept = 0;

  /**
   * @brief Forwards request to Transaction Manager for execution. See
   * TransactionManagerInterface.
   *
   * @param context
   * @return ExecutionResult
   */
  virtual core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          context) noexcept = 0;

  /**
   * @brief Forwards request to Transaction Manager for execution. See
   * TransactionManagerInterface.
   *
   * @param request
   * @param response
   * @return ExecutionResult
   */
  virtual core::ExecutionResult GetTransactionManagerStatus(
      const core::GetTransactionManagerStatusRequest& request,
      core::GetTransactionManagerStatusResponse& response) noexcept = 0;
};

}  // namespace google::scp::pbs
