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

#include "consume_budget_transaction_protocol.h"

#include <memory>
#include <string>
#include <utility>

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

namespace google::scp::pbs {
ExecutionResult ConsumeBudgetTransactionProtocol::Prepare(
    AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>&
        prepare_consume_budget_context) noexcept {
  if (prepare_consume_budget_context.request->transaction_id == kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  LoadBudgetKeyTimeframeRequest request = {
      .reporting_times = {prepare_consume_budget_context.request->time_bucket}};

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(move(request)),
          bind(&ConsumeBudgetTransactionProtocol::OnPrepareBudgetKeyLoaded,
               this, prepare_consume_budget_context, _1),
          prepare_consume_budget_context);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void ConsumeBudgetTransactionProtocol::OnPrepareBudgetKeyLoaded(
    AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>&
        prepare_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error while loading the budget key return the error.
  if (!load_budget_key_timeframe_context.result.Successful()) {
    prepare_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    prepare_consume_budget_context.Finish();
    return;
  }

  auto budget_key_frame =
      load_budget_key_timeframe_context.response->budget_key_frames.front();

  // In the prepare phase two checks need to be done:
  // 1) Ensure there is no active write operation happening.
  // 2) Enough budget is available for the proposed operation.
  if (budget_key_frame->active_transaction_id.load() != kZeroUuid) {
    prepare_consume_budget_context.result = RetryExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS);
    prepare_consume_budget_context.Finish();
    return;
  }

  if (budget_key_frame->token_count <
      prepare_consume_budget_context.request->token_count) {
    prepare_consume_budget_context.result = FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET);
    prepare_consume_budget_context.Finish();
    return;
  }

  prepare_consume_budget_context.result = SuccessExecutionResult();
  prepare_consume_budget_context.Finish();
}

ExecutionResult ConsumeBudgetTransactionProtocol::Commit(
    AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>&
        commit_consume_budget_context) noexcept {
  if (commit_consume_budget_context.request->transaction_id == kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  LoadBudgetKeyTimeframeRequest request = {
      .reporting_times = {commit_consume_budget_context.request->time_bucket}};

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(move(request)),
          bind(&ConsumeBudgetTransactionProtocol::OnCommitBudgetKeyLoaded, this,
               commit_consume_budget_context, _1),
          commit_consume_budget_context);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void ConsumeBudgetTransactionProtocol::OnCommitBudgetKeyLoaded(
    AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>&
        commit_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error return
  if (!load_budget_key_timeframe_context.result.Successful()) {
    commit_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    commit_consume_budget_context.Finish();
    return;
  }

  auto budget_key_frame =
      load_budget_key_timeframe_context.response->budget_key_frames.front();

  // In the commit phase, it is required to take the ownership of the cached
  // object. So the current transaction will try to change the transaction id on
  // the entry in the cache from zero to the request transaction id.
  core::common::Uuid zero = kZeroUuid;
  core::common::Uuid transaction_id =
      commit_consume_budget_context.request->transaction_id;

  // This is a retry and we should return success instead of continuing.
  if (budget_key_frame->active_transaction_id.load() == transaction_id) {
    commit_consume_budget_context.result = SuccessExecutionResult();
    commit_consume_budget_context.Finish();
    return;
  }

  // If this request can change the active transaction id to the request
  // transaction id, it means no other threads can pass this line. In the case
  // that the request is being retried, this can be done again.
  if (!budget_key_frame->active_transaction_id.compare_exchange_strong(
          zero, transaction_id)) {
    commit_consume_budget_context.result = RetryExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS);
    commit_consume_budget_context.Finish();
    return;
  }

  // Budget check needs to happen. There is a chance that a write operation
  // has happened between this request's prepare and commit phases.
  if (budget_key_frame->token_count <
      commit_consume_budget_context.request->token_count) {
    // TODO: Optimization: Release lock here.
    commit_consume_budget_context.result = FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET);
    commit_consume_budget_context.Finish();
    return;
  }

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.parent_activity_id =
      commit_consume_budget_context.activity_id;
  update_budget_key_timeframe_context.correlation_id =
      commit_consume_budget_context.correlation_id;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_token_count = budget_key_frame->token_count.load() -
                            commit_consume_budget_context.request->token_count;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.high =
      commit_consume_budget_context.request->transaction_id.high;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.low =
      commit_consume_budget_context.request->transaction_id.low;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = budget_key_frame->token_count;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = commit_consume_budget_context.request->time_bucket;
  update_budget_key_timeframe_context.callback =
      bind(&ConsumeBudgetTransactionProtocol::OnCommitLogged, this,
           budget_key_frame, commit_consume_budget_context, _1);

  auto execution_result = budget_key_timeframe_manager_->Update(
      update_budget_key_timeframe_context);
  if (!execution_result.Successful()) {
    // TODO: Optimization: Release the lock here.
    commit_consume_budget_context.result = execution_result;
    commit_consume_budget_context.Finish();
    return;
  }
}

void ConsumeBudgetTransactionProtocol::OnCommitLogged(
    shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
    AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>&
        commit_consume_budget_context,
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context) noexcept {
  // TODO: The following update is redundant since this already
  // has been done inside the Update().
  if (update_budget_key_timeframe_context.result.Successful()) {
    budget_key_time_frame->active_token_count =
        update_budget_key_timeframe_context.request->timeframes_to_update.back()
            .active_token_count;
    budget_key_time_frame->active_transaction_id =
        update_budget_key_timeframe_context.request->timeframes_to_update.back()
            .active_transaction_id;
  }

  commit_consume_budget_context.result =
      update_budget_key_timeframe_context.result;
  commit_consume_budget_context.Finish();
}

ExecutionResult ConsumeBudgetTransactionProtocol::Notify(
    AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>&
        notify_consume_budget_context) noexcept {
  if (notify_consume_budget_context.request->transaction_id == kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  LoadBudgetKeyTimeframeRequest request = {
      .reporting_times = {notify_consume_budget_context.request->time_bucket}};

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(move(request)),
          bind(&ConsumeBudgetTransactionProtocol::OnNotifyBudgetKeyLoaded, this,
               notify_consume_budget_context, _1),
          notify_consume_budget_context);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void ConsumeBudgetTransactionProtocol::OnNotifyBudgetKeyLoaded(
    AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>&
        notify_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error return
  if (!load_budget_key_timeframe_context.result.Successful()) {
    notify_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    notify_consume_budget_context.Finish();
    return;
  }

  // Reset transaction id and swap active token value with the actual token.
  auto budget_key_time_frame =
      load_budget_key_timeframe_context.response->budget_key_frames.front();
  // TODO
  // Make an abstraction on the budget key time frame to Lock(txn_id) and
  // Unlock(txn_id) and check HoldsLock(txn_id).
  //
  // Ensure that the request has arrived with the right transaction id. If the
  // id is different there is a chance that the current key was notified already
  // and this is a retry operation.
  if (budget_key_time_frame->active_transaction_id.load() !=
      notify_consume_budget_context.request->transaction_id) {
    notify_consume_budget_context.result = SuccessExecutionResult();
    notify_consume_budget_context.Finish();
    return;
  }

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.activity_id =
      notify_consume_budget_context.activity_id;
  update_budget_key_timeframe_context.correlation_id =
      notify_consume_budget_context.correlation_id;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  BudgetKeyTimeframeUpdateInfo timeframe_to_update;
  timeframe_to_update.active_token_count = 0;
  timeframe_to_update.active_transaction_id.high = 0;
  timeframe_to_update.active_transaction_id.low = 0;
  timeframe_to_update.token_count = budget_key_time_frame->active_token_count;
  timeframe_to_update.reporting_time =
      notify_consume_budget_context.request->time_bucket;
  update_budget_key_timeframe_context.request->timeframes_to_update = {
      timeframe_to_update};
  update_budget_key_timeframe_context.callback =
      bind(&ConsumeBudgetTransactionProtocol::OnNotifyLogged, this,
           budget_key_time_frame, notify_consume_budget_context, _1);

  auto execution_result = budget_key_timeframe_manager_->Update(
      update_budget_key_timeframe_context);
  if (!execution_result.Successful()) {
    notify_consume_budget_context.result = execution_result;
    notify_consume_budget_context.Finish();
    return;
  }
}

void ConsumeBudgetTransactionProtocol::OnNotifyLogged(
    shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
    AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>&
        notify_consume_budget_context,
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context) noexcept {
  // TODO: The following update is redundant since this already
  // has been done inside the Update().
  if (update_budget_key_timeframe_context.result.Successful()) {
    budget_key_time_frame->token_count =
        update_budget_key_timeframe_context.request->timeframes_to_update.back()
            .token_count;
    budget_key_time_frame->active_token_count = 0;
    budget_key_time_frame->active_transaction_id = kZeroUuid;
  }

  notify_consume_budget_context.result =
      update_budget_key_timeframe_context.result;
  notify_consume_budget_context.Finish();
}

ExecutionResult ConsumeBudgetTransactionProtocol::Abort(
    AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
        abort_consume_budget_context) noexcept {
  if (abort_consume_budget_context.request->transaction_id == kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  LoadBudgetKeyTimeframeRequest request = {
      .reporting_times = {abort_consume_budget_context.request->time_bucket}};

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(move(request)),
          bind(&ConsumeBudgetTransactionProtocol::OnAbortBudgetKeyLoaded, this,
               abort_consume_budget_context, _1),
          abort_consume_budget_context);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void ConsumeBudgetTransactionProtocol::OnAbortBudgetKeyLoaded(
    AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
        abort_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error return
  if (!load_budget_key_timeframe_context.result.Successful()) {
    abort_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    abort_consume_budget_context.Finish();
    return;
  }

  // Reset transaction id and swap active token value with the actual token.
  auto budget_key_frame =
      load_budget_key_timeframe_context.response->budget_key_frames.front();

  // Ensure that the request has arrived with the right transaction id.
  if (budget_key_frame->active_transaction_id.load() !=
      abort_consume_budget_context.request->transaction_id) {
    abort_consume_budget_context.result = SuccessExecutionResult();
    abort_consume_budget_context.Finish();
    return;
  }

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.parent_activity_id =
      load_budget_key_timeframe_context.activity_id;
  update_budget_key_timeframe_context.correlation_id =
      load_budget_key_timeframe_context.correlation_id;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  BudgetKeyTimeframeUpdateInfo timeframe_to_update;
  timeframe_to_update.active_token_count = 0;
  timeframe_to_update.active_transaction_id.high = 0;
  timeframe_to_update.active_transaction_id.low = 0;
  timeframe_to_update.token_count = budget_key_frame->token_count;
  timeframe_to_update.reporting_time =
      abort_consume_budget_context.request->time_bucket;
  update_budget_key_timeframe_context.request->timeframes_to_update = {
      timeframe_to_update};
  update_budget_key_timeframe_context.callback =
      bind(&ConsumeBudgetTransactionProtocol::OnAbortLogged, this,
           budget_key_frame, abort_consume_budget_context, _1);

  auto execution_result = budget_key_timeframe_manager_->Update(
      update_budget_key_timeframe_context);
  if (!execution_result.Successful()) {
    abort_consume_budget_context.result = execution_result;
    abort_consume_budget_context.Finish();
    return;
  }
}

void ConsumeBudgetTransactionProtocol::OnAbortLogged(
    shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
    AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
        abort_consume_budget_context,
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context) noexcept {
  // TODO: The following update is redundant since this already
  // has been done inside the Update().
  if (update_budget_key_timeframe_context.result.Successful()) {
    // Ensure that the request has arrived with the right transaction id.
    if (budget_key_time_frame->active_transaction_id.load() ==
        abort_consume_budget_context.request->transaction_id) {
      budget_key_time_frame->active_token_count =
          update_budget_key_timeframe_context.request->timeframes_to_update
              .back()
              .active_token_count;
      budget_key_time_frame->active_transaction_id =
          update_budget_key_timeframe_context.request->timeframes_to_update
              .back()
              .active_transaction_id;
    }
  }

  abort_consume_budget_context.result =
      update_budget_key_timeframe_context.result;
  abort_consume_budget_context.Finish();
}

}  // namespace google::scp::pbs
