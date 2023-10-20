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

#include "remote_transaction_manager.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/http2_client.h"
#include "pbs/front_end_service/src/front_end_utils.h"
#include "pbs/interface/type_def.h"
#include "pbs/pbs_client/src/pbs_client.h"

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::CredentialsProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::pbs::PrivacyBudgetServiceClient;
using std::bind;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::pbs {
RemoteTransactionManager::RemoteTransactionManager(
    const std::shared_ptr<PrivacyBudgetServiceClientInterface>& pbs_client)
    : pbs_client_(pbs_client) {}

ExecutionResult RemoteTransactionManager::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult RemoteTransactionManager::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult RemoteTransactionManager::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult RemoteTransactionManager::GetTransactionStatus(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context) noexcept {
  return pbs_client_->GetTransactionStatus(get_transaction_status_context);
}

ExecutionResult RemoteTransactionManager::ExecutePhase(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        transaction_phase_context) noexcept {
  return pbs_client_->ExecuteTransactionPhase(transaction_phase_context);
}

}  //  namespace google::scp::pbs
