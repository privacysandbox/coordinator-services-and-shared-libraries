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

#include "batch_consume_budget_command.h"

#include <functional>
#include <memory>
#include <set>
#include <utility>
#include <vector>

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
using std::set;
using std::vector;
using std::placeholders::_1;

namespace google::scp::pbs {

template <class Request, class Response>
void BatchConsumeBudgetCommand::SetFailedInsufficientBudgetConsumptions(
    const AsyncContext<Request, Response>& context) {
  failed_insufficient_budget_consumptions_.clear();
  set<TimeBucket> timebuckets_with_insufficient_budget;
  for (const auto& failed_index :
       context.response->failed_budget_consumption_indices) {
    timebuckets_with_insufficient_budget.insert(
        context.request->budget_consumptions[failed_index].time_bucket);
  }
  for (const auto& budget_consumption : budget_consumptions_) {
    if ((timebuckets_with_insufficient_budget.find(
            budget_consumption.time_bucket)) !=
        timebuckets_with_insufficient_budget.end()) {
      failed_insufficient_budget_consumptions_.push_back(budget_consumption);
    }
  }
}

ExecutionResult BatchConsumeBudgetCommand::Prepare(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&BatchConsumeBudgetCommand::OnPrepareGetBudgetKeyCallback, this, _1,
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

void BatchConsumeBudgetCommand::OnPrepareGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (get_budget_key_context.result != SuccessExecutionResult()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol =
      get_budget_key_context.response->budget_key
          ->GetBatchBudgetConsumptionTransactionProtocol();

  PrepareBatchConsumeBudgetRequest prepare_batch_consume_budget_request = {
      .transaction_id = transaction_id_};

  for (const auto& budget_consumption : budget_consumptions_) {
    BudgetConsumptionRequestInfo budget_consumption_request = {
        .time_bucket = budget_consumption.time_bucket,
        .token_count = budget_consumption.token_count};
    prepare_batch_consume_budget_request.budget_consumptions.push_back(
        move(budget_consumption_request));
  }

  AsyncContext<PrepareBatchConsumeBudgetRequest,
               PrepareBatchConsumeBudgetResponse>
      prepare_batch_consume_budget_context(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          bind(&BatchConsumeBudgetCommand::OnPrepareBatchConsumeBudgetCallback,
               this, _1, transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<AsyncContext<
      PrepareBatchConsumeBudgetRequest, PrepareBatchConsumeBudgetResponse>>(
      prepare_batch_consume_budget_context,
      [transaction_protocol](AsyncContext<PrepareBatchConsumeBudgetRequest,
                                          PrepareBatchConsumeBudgetResponse>&
                                 prepare_batch_consume_budget_context) {
        return transaction_protocol->Prepare(
            prepare_batch_consume_budget_context);
      });
}

void BatchConsumeBudgetCommand::OnPrepareBatchConsumeBudgetCallback(
    AsyncContext<PrepareBatchConsumeBudgetRequest,
                 PrepareBatchConsumeBudgetResponse>&
        prepare_batch_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if ((prepare_batch_consume_budget_context.result ==
       FailureExecutionResult(
           core::errors::
               SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET)) &&
      prepare_batch_consume_budget_context.response) {
    SetFailedInsufficientBudgetConsumptions(
        prepare_batch_consume_budget_context);
  }
  transaction_command_callback(prepare_batch_consume_budget_context.result);
}

ExecutionResult BatchConsumeBudgetCommand::Commit(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&BatchConsumeBudgetCommand::OnCommitGetBudgetKeyCallback, this, _1,
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

void BatchConsumeBudgetCommand::OnCommitGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (get_budget_key_context.result != SuccessExecutionResult()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol =
      get_budget_key_context.response->budget_key
          ->GetBatchBudgetConsumptionTransactionProtocol();

  CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request = {
      .transaction_id = transaction_id_};

  for (const auto& budget_consumption : budget_consumptions_) {
    BudgetConsumptionRequestInfo budget_consumption_request = {
        .time_bucket = budget_consumption.time_bucket,
        .token_count = budget_consumption.token_count};
    commit_batch_consume_budget_request.budget_consumptions.push_back(
        move(budget_consumption_request));
  }

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_batch_consume_budget_context(
          make_shared<CommitBatchConsumeBudgetRequest>(
              move(commit_batch_consume_budget_request)),
          bind(&BatchConsumeBudgetCommand::OnCommitBatchConsumeBudgetCallback,
               this, _1, transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<AsyncContext<
      CommitBatchConsumeBudgetRequest, CommitBatchConsumeBudgetResponse>>(
      commit_batch_consume_budget_context,
      [transaction_protocol](AsyncContext<CommitBatchConsumeBudgetRequest,
                                          CommitBatchConsumeBudgetResponse>&
                                 commit_batch_consume_budget_context) {
        return transaction_protocol->Commit(
            commit_batch_consume_budget_context);
      });
}

void BatchConsumeBudgetCommand::OnCommitBatchConsumeBudgetCallback(
    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>&
        commit_batch_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if ((commit_batch_consume_budget_context.result ==
       FailureExecutionResult(
           core::errors::
               SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET)) &&
      commit_batch_consume_budget_context.response) {
    SetFailedInsufficientBudgetConsumptions(
        commit_batch_consume_budget_context);
  }
  transaction_command_callback(commit_batch_consume_budget_context.result);
}

ExecutionResult BatchConsumeBudgetCommand::Notify(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&BatchConsumeBudgetCommand::OnNotifyGetBudgetKeyCallback, this, _1,
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

void BatchConsumeBudgetCommand::OnNotifyGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (get_budget_key_context.result != SuccessExecutionResult()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol =
      get_budget_key_context.response->budget_key
          ->GetBatchBudgetConsumptionTransactionProtocol();

  NotifyBatchConsumeBudgetRequest notify_batch_consume_budget_request = {
      .transaction_id = transaction_id_};

  for (const auto& budget_consumption : budget_consumptions_) {
    notify_batch_consume_budget_request.time_buckets.push_back(
        budget_consumption.time_bucket);
  }

  AsyncContext<NotifyBatchConsumeBudgetRequest,
               NotifyBatchConsumeBudgetResponse>
      notify_batch_consume_budget_context(
          make_shared<NotifyBatchConsumeBudgetRequest>(
              move(notify_batch_consume_budget_request)),
          bind(&BatchConsumeBudgetCommand::OnNotifyBatchConsumeBudgetCallback,
               this, _1, transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<AsyncContext<
      NotifyBatchConsumeBudgetRequest, NotifyBatchConsumeBudgetResponse>>(
      notify_batch_consume_budget_context,
      [transaction_protocol](AsyncContext<NotifyBatchConsumeBudgetRequest,
                                          NotifyBatchConsumeBudgetResponse>&
                                 notify_batch_consume_budget_context) {
        return transaction_protocol->Notify(
            notify_batch_consume_budget_context);
      });
}

void BatchConsumeBudgetCommand::OnNotifyBatchConsumeBudgetCallback(
    AsyncContext<NotifyBatchConsumeBudgetRequest,
                 NotifyBatchConsumeBudgetResponse>&
        notify_batch_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  transaction_command_callback(notify_batch_consume_budget_context.result);
}

ExecutionResult BatchConsumeBudgetCommand::Abort(
    TransactionCommandCallback& transaction_command_callback) noexcept {
  GetBudgetKeyRequest get_budget_key_request = {.budget_key_name =
                                                    budget_key_name_};
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse> budget_key_context(
      make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
      bind(&BatchConsumeBudgetCommand::OnAbortGetBudgetKeyCallback, this, _1,
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

void BatchConsumeBudgetCommand::OnAbortGetBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  if (get_budget_key_context.result != SuccessExecutionResult()) {
    transaction_command_callback(get_budget_key_context.result);
    return;
  }

  auto transaction_protocol =
      get_budget_key_context.response->budget_key
          ->GetBatchBudgetConsumptionTransactionProtocol();

  AbortBatchConsumeBudgetRequest abort_batch_consume_budget_request = {
      .transaction_id = transaction_id_};

  for (const auto& budget_consumption : budget_consumptions_) {
    abort_batch_consume_budget_request.time_buckets.push_back(
        budget_consumption.time_bucket);
  }

  AsyncContext<AbortBatchConsumeBudgetRequest, AbortBatchConsumeBudgetResponse>
      abort_batch_consume_budget_context(
          make_shared<AbortBatchConsumeBudgetRequest>(
              move(abort_batch_consume_budget_request)),
          bind(&BatchConsumeBudgetCommand::OnAbortBatchConsumeBudgetCallback,
               this, _1, transaction_command_callback),
          transaction_id_);

  operation_dispatcher_.Dispatch<AsyncContext<AbortBatchConsumeBudgetRequest,
                                              AbortBatchConsumeBudgetResponse>>(
      abort_batch_consume_budget_context,
      [transaction_protocol](AsyncContext<AbortBatchConsumeBudgetRequest,
                                          AbortBatchConsumeBudgetResponse>&
                                 abort_batch_consume_budget_context) {
        return transaction_protocol->Abort(abort_batch_consume_budget_context);
      });
}

void BatchConsumeBudgetCommand::OnAbortBatchConsumeBudgetCallback(
    AsyncContext<AbortBatchConsumeBudgetRequest,
                 AbortBatchConsumeBudgetResponse>&
        abort_batch_consume_budget_context,
    TransactionCommandCallback& transaction_command_callback) noexcept {
  transaction_command_callback(abort_batch_consume_budget_context.result);
}

}  // namespace google::scp::pbs
