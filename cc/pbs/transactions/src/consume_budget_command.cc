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

#include "consume_budget_command.h"

#include <functional>
#include <memory>
#include <utility>

#include "core/interface/async_context.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/budget_key_transaction_protocols/src/error_codes.h"
#include "pbs/interface/budget_key_provider_interface.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionCommandCallback;
using google::scp::pbs::GetBudgetKeyRequest;
using std::bind;
using std::make_shared;
using std::move;
using std::placeholders::_1;

namespace google::scp::pbs {

ExecutionResult ConsumeBudgetCommand::Prepare(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&ConsumeBudgetCommand::OnPrepareGetBudgetKeyCallback, this, _1,
           transaction_command_callback),
      transaction_id_);

  operation_dispatcher_
      .Dispatch<AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>>(
          budget_key_context,
          [budget_key_provider = budget_key_provider_](
              AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                  budget_key_context) {
            return budget_key_provider->GetBudgetKey(budget_key_context);
          });
  return SuccessExecutionResult();
}

void ConsumeBudgetCommand::OnPrepareGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (!get_budget_key_context.result.Successful()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol = get_budget_key_context.response->budget_key
                                  ->GetBudgetConsumptionTransactionProtocol();

  PrepareConsumeBudgetRequest prepare_consume_budget_request{
      .transaction_id = transaction_id_,
      .time_bucket = budget_consumption_.time_bucket,
      .token_count = budget_consumption_.token_count,
  };

  AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>
      prepare_consume_budget_context(
          make_shared<PrepareConsumeBudgetRequest>(
              move(prepare_consume_budget_request)),
          bind(&ConsumeBudgetCommand::OnPrepareConsumeBudgetCallback, this, _1,
               transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<
      AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>>(
      prepare_consume_budget_context,
      [transaction_protocol](AsyncContext<PrepareConsumeBudgetRequest,
                                          PrepareConsumeBudgetResponse>&
                                 prepare_consume_budget_context) {
        return transaction_protocol->Prepare(prepare_consume_budget_context);
      });
}

void ConsumeBudgetCommand::OnPrepareConsumeBudgetCallback(
    AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>&
        prepare_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  failed_with_insufficient_budget_consumption_ =
      (prepare_consume_budget_context.result ==
       FailureExecutionResult(
           core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
  transaction_command_callback(prepare_consume_budget_context.result);
}

ExecutionResult ConsumeBudgetCommand::Commit(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&ConsumeBudgetCommand::OnCommitGetBudgetKeyCallback, this, _1,
           transaction_command_callback),
      transaction_id_);

  operation_dispatcher_
      .Dispatch<AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>>(
          budget_key_context,
          [budget_key_provider = budget_key_provider_](
              AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                  budget_key_context) {
            return budget_key_provider->GetBudgetKey(budget_key_context);
          });
  return SuccessExecutionResult();
}

void ConsumeBudgetCommand::OnCommitGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (!get_budget_key_context.result.Successful()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol = get_budget_key_context.response->budget_key
                                  ->GetBudgetConsumptionTransactionProtocol();

  CommitConsumeBudgetRequest commit_consume_budget_request{
      .transaction_id = transaction_id_,
      .time_bucket = budget_consumption_.time_bucket,
      .token_count = budget_consumption_.token_count,
  };

  AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
      commit_consume_budget_context(
          make_shared<CommitConsumeBudgetRequest>(
              move(commit_consume_budget_request)),
          bind(&ConsumeBudgetCommand::OnCommitConsumeBudgetCallback, this, _1,
               transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<
      AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>>(
      commit_consume_budget_context,
      [transaction_protocol](
          AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>&
              commit_consume_budget_context) {
        return transaction_protocol->Commit(commit_consume_budget_context);
      });
}

void ConsumeBudgetCommand::OnCommitConsumeBudgetCallback(
    AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>&
        commit_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  failed_with_insufficient_budget_consumption_ =
      (commit_consume_budget_context.result ==
       FailureExecutionResult(
           core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
  transaction_command_callback(commit_consume_budget_context.result);
}

ExecutionResult ConsumeBudgetCommand::Notify(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&ConsumeBudgetCommand::OnNotifyGetBudgetKeyCallback, this, _1,
           transaction_command_callback),
      transaction_id_);

  operation_dispatcher_
      .Dispatch<AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>>(
          budget_key_context,
          [budget_key_provider = budget_key_provider_](
              AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                  budget_key_context) {
            return budget_key_provider->GetBudgetKey(budget_key_context);
          });
  return SuccessExecutionResult();
};

void ConsumeBudgetCommand::OnNotifyGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (!get_budget_key_context.result.Successful()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol = get_budget_key_context.response->budget_key
                                  ->GetBudgetConsumptionTransactionProtocol();

  NotifyConsumeBudgetRequest notify_consume_budget_request{
      .transaction_id = transaction_id_,
      .time_bucket = budget_consumption_.time_bucket};

  AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>
      notify_consume_budget_context(
          make_shared<NotifyConsumeBudgetRequest>(
              move(notify_consume_budget_request)),
          bind(&ConsumeBudgetCommand::OnNotifyConsumeBudgetCallback, this, _1,
               transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<
      AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>>(
      notify_consume_budget_context,
      [transaction_protocol](
          AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>&
              notify_consume_budget_context) {
        return transaction_protocol->Notify(notify_consume_budget_context);
      });
}

void ConsumeBudgetCommand::OnNotifyConsumeBudgetCallback(
    AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>&
        notify_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  transaction_command_callback(notify_consume_budget_context.result);
}

ExecutionResult ConsumeBudgetCommand::Abort(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&ConsumeBudgetCommand::OnAbortGetBudgetKeyCallback, this, _1,
           transaction_command_callback),
      transaction_id_);

  operation_dispatcher_
      .Dispatch<AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>>(
          budget_key_context,
          [budget_key_provider = budget_key_provider_](
              AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                  budget_key_context) {
            return budget_key_provider->GetBudgetKey(budget_key_context);
          });
  return SuccessExecutionResult();
};

void ConsumeBudgetCommand::OnAbortGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (!get_budget_key_context.result.Successful()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol = get_budget_key_context.response->budget_key
                                  ->GetBudgetConsumptionTransactionProtocol();

  AbortConsumeBudgetRequest abort_consume_budget_request{
      .transaction_id = transaction_id_,
      .time_bucket = budget_consumption_.time_bucket};

  AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>
      abort_consume_budget_context(
          make_shared<AbortConsumeBudgetRequest>(
              move(abort_consume_budget_request)),
          bind(&ConsumeBudgetCommand::OnAbortConsumeBudgetCallback, this, _1,
               transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<
      AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>>(
      abort_consume_budget_context,
      [transaction_protocol](
          AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
              abort_consume_budget_context) {
        return transaction_protocol->Abort(abort_consume_budget_context);
      });
}

void ConsumeBudgetCommand::OnAbortConsumeBudgetCallback(
    AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
        abort_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  transaction_command_callback(abort_consume_budget_context.result);
}
}  // namespace google::scp::pbs
