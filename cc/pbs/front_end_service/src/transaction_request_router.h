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

#include "core/interface/transaction_manager_interface.h"
#include "core/interface/transaction_request_router_interface.h"

namespace google::scp::pbs {
/**
 * @copydoc TransactionRequestRouterInterface
 *
 */
class TransactionRequestRouter
    : public core::TransactionRequestRouterInterface {
 public:
  TransactionRequestRouter(
      const std::shared_ptr<core::TransactionManagerInterface>&
          transaction_manager)
      : transaction_manager_(transaction_manager) {}

  core::ExecutionResult Execute(
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          context) noexcept override;

  core::ExecutionResult Execute(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>& context) noexcept
      override;

  core::ExecutionResult Execute(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>& context) noexcept
      override;

  core::ExecutionResult Execute(
      const core::GetTransactionManagerStatusRequest& request,
      core::GetTransactionManagerStatusResponse& response) noexcept override;

 protected:
  std::shared_ptr<core::TransactionManagerInterface> transaction_manager_;
};
}  // namespace google::scp::pbs
