// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "client_consume_budget_command.h"

#include <functional>
#include <memory>

#include "core/http2_client/src/error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionCommandCallback;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using std::bind;
using std::make_shared;
using std::shared_ptr;
using std::placeholders::_1;

static constexpr char kClientConsumeBudgetCommand[] =
    "ClientConsumeBudgetCommand";

namespace google::scp::pbs {
ExecutionResult ClientConsumeBudgetCommand::Begin(
    TransactionCommandCallback& callback) noexcept {
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context(
          make_shared<ConsumeBudgetTransactionRequest>(),
          bind(&ClientConsumeBudgetCommand::
                   OnInitiateConsumeBudgetTransactionCallback,
               this, _1, callback),
          parent_activity_id_, parent_activity_id_);

  consume_budget_transaction_context.request->transaction_id = transaction_id_;
  consume_budget_transaction_context.request->transaction_secret =
      transaction_secret_;
  consume_budget_transaction_context.request->budget_keys = budget_keys_;

  operation_dispatcher_.Dispatch<AsyncContext<
      ConsumeBudgetTransactionRequest, ConsumeBudgetTransactionResponse>>(
      consume_budget_transaction_context,
      [pbs_client = pbs_client_](AsyncContext<ConsumeBudgetTransactionRequest,
                                              ConsumeBudgetTransactionResponse>&
                                     consume_budget_transaction_context) {
        return pbs_client->InitiateConsumeBudgetTransaction(
            consume_budget_transaction_context);
      });

  auto transaction_id_string = core::common::ToString(transaction_id_);
  auto command_id_string = core::common::ToString(command_id);
  SCP_DEBUG_CONTEXT(kClientConsumeBudgetCommand,
                    consume_budget_transaction_context,
                    "Begin transaction for command id: %s transaction id: %s",
                    command_id_string.c_str(), transaction_id_string.c_str());

  return SuccessExecutionResult();
}

void ClientConsumeBudgetCommand::OnInitiateConsumeBudgetTransactionCallback(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context,
    TransactionCommandCallback& callback) noexcept {
  if (consume_budget_transaction_context.result.Successful()) {
    last_execution_timestamp_ =
        consume_budget_transaction_context.response->last_execution_timestamp;
  }

  auto transaction_id_string = core::common::ToString(transaction_id_);
  auto command_id_string = core::common::ToString(command_id);
  SCP_DEBUG_CONTEXT(
      kClientConsumeBudgetCommand, consume_budget_transaction_context,
      "Begin transaction callback for command id: %s transaction id: %s last "
      "execution time: %llu",
      command_id_string.c_str(), transaction_id_string.c_str(),
      last_execution_timestamp_);

  callback(consume_budget_transaction_context.result);
}

ExecutionResult ClientConsumeBudgetCommand::Prepare(
    TransactionCommandCallback& callback) noexcept {
  return ExecuteTransactionPhase(TransactionExecutionPhase::Prepare, callback);
}

ExecutionResult ClientConsumeBudgetCommand::Commit(
    TransactionCommandCallback& callback) noexcept {
  return ExecuteTransactionPhase(TransactionExecutionPhase::Commit, callback);
}

ExecutionResult ClientConsumeBudgetCommand::Notify(
    TransactionCommandCallback& callback) noexcept {
  return ExecuteTransactionPhase(TransactionExecutionPhase::Notify, callback);
}

ExecutionResult ClientConsumeBudgetCommand::Abort(
    TransactionCommandCallback& callback) noexcept {
  return ExecuteTransactionPhase(TransactionExecutionPhase::Abort, callback);
}

ExecutionResult ClientConsumeBudgetCommand::End(
    TransactionCommandCallback& callback) noexcept {
  return ExecuteTransactionPhase(TransactionExecutionPhase::End, callback);
}

ExecutionResult ClientConsumeBudgetCommand::ExecuteTransactionPhase(
    TransactionExecutionPhase transaction_execution_phase,
    TransactionCommandCallback& transaction_phase_callback) noexcept {
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          bind(&ClientConsumeBudgetCommand::OnPhaseExecutionCallback, this, _1,
               transaction_phase_callback),
          parent_activity_id_, parent_activity_id_);

  auto transaction_id_string = core::common::ToString(transaction_id_);
  auto command_id_string = core::common::ToString(command_id);
  SCP_DEBUG_CONTEXT(
      kClientConsumeBudgetCommand, transaction_phase_context,
      "Executing transaction phase for command id: %s transaction "
      "id: %s last "
      "execution time: %llu",
      command_id_string.c_str(), transaction_id_string.c_str(),
      last_execution_timestamp_);

  transaction_phase_context.request->transaction_id = transaction_id_;
  transaction_phase_context.request->transaction_secret = transaction_secret_;
  transaction_phase_context.request->last_execution_timestamp =
      last_execution_timestamp_;
  transaction_phase_context.request->transaction_execution_phase =
      transaction_execution_phase;

  operation_dispatcher_.Dispatch<
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>>(
      transaction_phase_context,
      [pbs_client = pbs_client_](
          AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        return pbs_client->ExecuteTransactionPhase(transaction_phase_context);
      });

  return SuccessExecutionResult();
}

void ClientConsumeBudgetCommand::OnPhaseExecutionCallback(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        transaction_phase_context,
    TransactionCommandCallback& transaction_phase_callback) noexcept {
  if (transaction_phase_context.result.status_code ==
      core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_PRECONDITION_FAILED) {
    // Get status and then continue from the last phase.
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
        get_transaction_status_context(
            make_shared<GetTransactionStatusRequest>(),
            bind(&ClientConsumeBudgetCommand::
                     OnExecuteTransactionPhaseGetStatusCallback,
                 this, transaction_phase_context, _1,
                 transaction_phase_callback),
            transaction_phase_context);

    get_transaction_status_context.request->transaction_id =
        transaction_phase_context.request->transaction_id;
    get_transaction_status_context.request->transaction_secret =
        transaction_phase_context.request->transaction_secret;

    auto execution_result =
        pbs_client_->GetTransactionStatus(get_transaction_status_context);
    if (!execution_result.Successful()) {
      transaction_phase_callback(execution_result);
    }
    return;
  }

  if (transaction_phase_context.result.Successful()) {
    last_execution_timestamp_ =
        transaction_phase_context.response->last_execution_timestamp;
  }

  auto transaction_id_string = core::common::ToString(transaction_id_);
  auto command_id_string = core::common::ToString(command_id);
  SCP_DEBUG_CONTEXT(
      kClientConsumeBudgetCommand, transaction_phase_context,
      "OnPhaseExecutionCallback for command id: %s transaction id: %s last "
      "execution time: %llu",
      command_id_string.c_str(), transaction_id_string.c_str(),
      last_execution_timestamp_);

  transaction_phase_callback(transaction_phase_context.result);
}

void ClientConsumeBudgetCommand::OnExecuteTransactionPhaseGetStatusCallback(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        transaction_phase_context,
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context,
    TransactionCommandCallback& transaction_phase_callback) noexcept {
  if (!get_transaction_status_context.result.Successful()) {
    transaction_phase_callback(get_transaction_status_context.result);
    return;
  }

  last_execution_timestamp_ =
      get_transaction_status_context.response->last_execution_timestamp;
  auto execution_result = ExecuteTransactionPhase(
      transaction_phase_context.request->transaction_execution_phase,
      transaction_phase_callback);
  if (!execution_result.Successful()) {
    transaction_phase_callback(execution_result);
  }
}
}  // namespace google::scp::pbs
