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

#include "pbs/front_end_service/src/transaction_request_router.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;

namespace google::scp::pbs {
ExecutionResult TransactionRequestRouter::Execute(
    AsyncContext<TransactionRequest, TransactionResponse>& context) noexcept {
  return transaction_manager_->Execute(context);
}

ExecutionResult TransactionRequestRouter::Execute(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        context) noexcept {
  return transaction_manager_->ExecutePhase(context);
}

ExecutionResult TransactionRequestRouter::Execute(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        context) noexcept {
  return transaction_manager_->GetTransactionStatus(context);
}

ExecutionResult TransactionRequestRouter::Execute(
    const GetTransactionManagerStatusRequest& request,
    GetTransactionManagerStatusResponse& response) noexcept {
  return transaction_manager_->GetTransactionManagerStatus(request, response);
}

}  // namespace google::scp::pbs
