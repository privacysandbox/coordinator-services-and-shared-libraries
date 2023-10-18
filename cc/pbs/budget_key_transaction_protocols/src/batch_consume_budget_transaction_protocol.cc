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

#include "batch_consume_budget_transaction_protocol.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "error_codes.h"
#include "transaction_protocol_helpers.h"

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
using std::vector;
using std::placeholders::_1;

// Transaction Protocols run within the Transaction Manager component.
static constexpr char kTransactionProtocol[] = "TransactionManager";

namespace google::scp::pbs {

/**
 * @brief Populate the indices of the Request's BudgetConsumptions in the
 * Response which have exceeded their budgets.
 *
 * This needs to be invoked in the Failure path of the Prepare/Commit response.
 *
 * @tparam Request
 * @tparam Response
 * @param load_frames_context
 * @param context
 * @return ExecutionResult
 */
template <typename Request, typename Response>
ExecutionResult PopulateInsufficientBudgetConsumptionIndicesInResponse(
    const AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>& load_frames_context,
    AsyncContext<Request, Response>& context) {
  if (context.request->budget_consumptions.size() !=
      load_frames_context.response->budget_key_frames.size()) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_BUDGET_KEY_CONSUME_BUDGET_LOADED_TIMEFRAMES_INVALID);
  }
  if (!context.response) {
    context.response = make_shared<Response>();
  }
  context.response->failed_budget_consumption_indices.clear();
  for (int i = 0; i < context.request->budget_consumptions.size(); i++) {
    if (load_frames_context.response->budget_key_frames[i]->token_count <
        context.request->budget_consumptions[i].token_count) {
      context.response->failed_budget_consumption_indices.push_back(i);
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult BatchConsumeBudgetTransactionProtocol::Prepare(
    AsyncContext<PrepareBatchConsumeBudgetRequest,
                 PrepareBatchConsumeBudgetResponse>&
        prepare_batch_consume_budget_context) noexcept {
  if (prepare_batch_consume_budget_context.request->transaction_id ==
      kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  if (prepare_batch_consume_budget_context.request->budget_consumptions
          .size() <= 1) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_LESS_BUDGETS_TO_CONSUME);
  }

  // Load all budget key timeframes for the budget time buckets specified
  LoadBudgetKeyTimeframeRequest load_timeframes_request;
  for (const auto& budget_consumption :
       prepare_batch_consume_budget_context.request->budget_consumptions) {
    load_timeframes_request.reporting_times.push_back(
        budget_consumption.time_bucket);
  }

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(
              move(load_timeframes_request)),
          bind(&BatchConsumeBudgetTransactionProtocol::OnPrepareBudgetKeyLoaded,
               this, prepare_batch_consume_budget_context, _1),
          prepare_batch_consume_budget_context);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void BatchConsumeBudgetTransactionProtocol::OnPrepareBudgetKeyLoaded(
    AsyncContext<PrepareBatchConsumeBudgetRequest,
                 PrepareBatchConsumeBudgetResponse>&
        prepare_batch_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error while loading the budget key return the error.
  if (load_budget_key_timeframe_context.result != SuccessExecutionResult()) {
    prepare_batch_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    prepare_batch_consume_budget_context.Finish();
    return;
  }

  // All of the requested time frames must be loaded
  if (load_budget_key_timeframe_context.response->budget_key_frames.size() !=
      prepare_batch_consume_budget_context.request->budget_consumptions
          .size()) {
    prepare_batch_consume_budget_context.result = FailureExecutionResult(
        core::errors::
            SC_PBS_BUDGET_KEY_CONSUME_BUDGET_LOADED_TIMEFRAMES_INVALID);
    prepare_batch_consume_budget_context.Finish();
    return;
  }

  // Once time frames are loaded, go through each of them and check if they can
  // be modified
  const auto& budget_consumptions =
      prepare_batch_consume_budget_context.request->budget_consumptions;
  int i = 0;
  for (const auto& budget_to_consume : budget_consumptions) {
    auto budget_key_frame =
        load_budget_key_timeframe_context.response->budget_key_frames[i];

    // In the prepare phase two checks need to be done:
    // 1) Ensure there is no active write operation happening.
    // 2) Enough budget is available for the proposed operation.
    if (budget_key_frame->active_transaction_id.load() != kZeroUuid) {
      prepare_batch_consume_budget_context.result = RetryExecutionResult(
          core::errors::SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS);
      prepare_batch_consume_budget_context.Finish();
      return;
    }

    if (budget_key_frame->token_count < budget_to_consume.token_count) {
      prepare_batch_consume_budget_context.result = FailureExecutionResult(
          core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET);
      if (auto result = PopulateInsufficientBudgetConsumptionIndicesInResponse(
              load_budget_key_timeframe_context,
              prepare_batch_consume_budget_context);
          !result.Successful()) {
        // Log and continue
        SCP_ERROR_CONTEXT(kTransactionProtocol,
                          prepare_batch_consume_budget_context, result,
                          "Cannot populate failed budget indices in response");
      }
      prepare_batch_consume_budget_context.Finish();
      return;
    }
    i++;
  }

  prepare_batch_consume_budget_context.result = SuccessExecutionResult();
  prepare_batch_consume_budget_context.Finish();
}

ExecutionResult BatchConsumeBudgetTransactionProtocol::Commit(
    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>&
        commit_batch_consume_budget_context) noexcept {
  if (commit_batch_consume_budget_context.request->transaction_id ==
      kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  if (commit_batch_consume_budget_context.request->budget_consumptions.size() <=
      1) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_LESS_BUDGETS_TO_CONSUME);
  }

  // To avoid livelock situation, only allow the requests with an increasing
  // order of time buckets.
  if (!TransactionProtocolHelpers::AreBudgetsInIncreasingOrder(
          commit_batch_consume_budget_context.request->budget_consumptions)) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_INVALID_ORDER);
  }

  // Load all budget key timeframes for the budget time buckets specified
  LoadBudgetKeyTimeframeRequest load_timeframes_request;
  for (const auto& budget_consumption :
       commit_batch_consume_budget_context.request->budget_consumptions) {
    load_timeframes_request.reporting_times.push_back(
        budget_consumption.time_bucket);
  }

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(
              move(load_timeframes_request)),
          bind(&BatchConsumeBudgetTransactionProtocol::OnCommitBudgetKeyLoaded,
               this, commit_batch_consume_budget_context, _1),
          commit_batch_consume_budget_context);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void BatchConsumeBudgetTransactionProtocol::OnCommitBudgetKeyLoaded(
    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>&
        commit_batch_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error return
  if (load_budget_key_timeframe_context.result != SuccessExecutionResult()) {
    commit_batch_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    commit_batch_consume_budget_context.Finish();
    return;
  }

  if (load_budget_key_timeframe_context.response->budget_key_frames.size() !=
      commit_batch_consume_budget_context.request->budget_consumptions.size()) {
    commit_batch_consume_budget_context.result = FailureExecutionResult(
        core::errors::
            SC_PBS_BUDGET_KEY_CONSUME_BUDGET_LOADED_TIMEFRAMES_INVALID);
    commit_batch_consume_budget_context.Finish();
    return;
  }

  // In the commit phase, it is required to take the ownership of the cached
  // timeframe objects. So the current transaction will try to change the
  // transaction id on each of the timeframe entries in the cache from zero to
  // the request transaction id.
  // If unable to acquire ownership on any of the timeframes, then return
  // failure.
  auto budget_key_timeframes =
      load_budget_key_timeframe_context.response->budget_key_frames;

  // Since all the timeframe locks are acquired or none, it is safe to look at
  // the first timeframe's lock to see if this is a retry request, i.e. some
  // previous attempt had already acquired all the locks.
  if (budget_key_timeframes[0]->active_transaction_id.load() ==
      commit_batch_consume_budget_context.request->transaction_id) {
    commit_batch_consume_budget_context.result = SuccessExecutionResult();
    commit_batch_consume_budget_context.Finish();
    return;
  }

  for (unsigned int i = 0; i < budget_key_timeframes.size(); i++) {
    core::common::Uuid zero = kZeroUuid;
    const core::common::Uuid transaction_id =
        commit_batch_consume_budget_context.request->transaction_id;

    // If this request can change the active transaction id to the request
    // transaction id, it means no other threads can pass this line for this
    // timeframe. In the case that the request is being retried, this can be
    // done again.
    if (!budget_key_timeframes[i]
             ->active_transaction_id.compare_exchange_strong(zero,
                                                             transaction_id)) {
      // If unable to acquire lock on one of the timeframes, the acquired locks
      // so far can be released safely as the timeframes have not been modified
      TransactionProtocolHelpers::ReleaseAcquiredLocksOnTimeframes(
          commit_batch_consume_budget_context.request->transaction_id,
          budget_key_timeframes);
      commit_batch_consume_budget_context.result = RetryExecutionResult(
          core::errors::SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS);
      commit_batch_consume_budget_context.Finish();
      return;
    }

    // Budget check needs to happen. There is a chance that a write operation
    // has happened between this request's prepare and commit phases.
    if (budget_key_timeframes[i]->token_count <
        commit_batch_consume_budget_context.request->budget_consumptions[i]
            .token_count) {
      // Locks can be safely released as timeframes are not yet modified
      TransactionProtocolHelpers::ReleaseAcquiredLocksOnTimeframes(
          commit_batch_consume_budget_context.request->transaction_id,
          budget_key_timeframes);
      commit_batch_consume_budget_context.result = FailureExecutionResult(
          core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET);
      if (auto result = PopulateInsufficientBudgetConsumptionIndicesInResponse(
              load_budget_key_timeframe_context,
              commit_batch_consume_budget_context);
          !result.Successful()) {
        // Log and continue
        SCP_ERROR_CONTEXT(kTransactionProtocol,
                          commit_batch_consume_budget_context, result,
                          "Cannot populate failed budget indices in response");
      }
      commit_batch_consume_budget_context.Finish();
      return;
    }
  }

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.parent_activity_id =
      commit_batch_consume_budget_context.activity_id;
  update_budget_key_timeframe_context.correlation_id =
      commit_batch_consume_budget_context.correlation_id;

  for (unsigned int i = 0; i < budget_key_timeframes.size(); i++) {
    update_budget_key_timeframe_context.request->timeframes_to_update
        .emplace_back();
    auto& timeframe_to_update = update_budget_key_timeframe_context.request
                                    ->timeframes_to_update.back();
    const auto& budget_consumption_request =
        commit_batch_consume_budget_context.request->budget_consumptions[i];

    timeframe_to_update.active_token_count =
        budget_key_timeframes[i]->token_count.load() -
        budget_consumption_request.token_count;
    timeframe_to_update.active_transaction_id.high =
        commit_batch_consume_budget_context.request->transaction_id.high;
    timeframe_to_update.active_transaction_id.low =
        commit_batch_consume_budget_context.request->transaction_id.low;
    timeframe_to_update.token_count = budget_key_timeframes[i]->token_count;
    timeframe_to_update.reporting_time = budget_consumption_request.time_bucket;
  }

  update_budget_key_timeframe_context.callback =
      bind(&BatchConsumeBudgetTransactionProtocol::OnCommitLogged, this,
           commit_batch_consume_budget_context, _1);

  auto execution_result = budget_key_timeframe_manager_->Update(
      update_budget_key_timeframe_context);
  if (execution_result != SuccessExecutionResult()) {
    // Update() did not make any modifications to underlying
    // data, locks can be released safely.
    TransactionProtocolHelpers::ReleaseAcquiredLocksOnTimeframes(
        commit_batch_consume_budget_context.request->transaction_id,
        budget_key_timeframes);
    commit_batch_consume_budget_context.result = execution_result;
    commit_batch_consume_budget_context.Finish();
    return;
  }
}

void BatchConsumeBudgetTransactionProtocol::OnCommitLogged(
    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>&
        commit_batch_consume_budget_context,
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context) noexcept {
  // Let Abort phase release the locks if there is a failure in update request
  commit_batch_consume_budget_context.result =
      update_budget_key_timeframe_context.result;
  commit_batch_consume_budget_context.Finish();
}

ExecutionResult BatchConsumeBudgetTransactionProtocol::Notify(
    AsyncContext<NotifyBatchConsumeBudgetRequest,
                 NotifyBatchConsumeBudgetResponse>&
        notify_batch_consume_budget_context) noexcept {
  if (notify_batch_consume_budget_context.request->transaction_id ==
      kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  if (notify_batch_consume_budget_context.request->time_buckets.size() <= 1) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_LESS_BUDGETS_TO_CONSUME);
  }

  LoadBudgetKeyTimeframeRequest request;
  request.reporting_times =
      notify_batch_consume_budget_context.request->time_buckets;

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(move(request)),
          bind(&BatchConsumeBudgetTransactionProtocol::OnNotifyBudgetKeyLoaded,
               this, notify_batch_consume_budget_context, _1),
          notify_batch_consume_budget_context);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void BatchConsumeBudgetTransactionProtocol::OnNotifyBudgetKeyLoaded(
    AsyncContext<NotifyBatchConsumeBudgetRequest,
                 NotifyBatchConsumeBudgetResponse>&
        notify_batch_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error return
  if (load_budget_key_timeframe_context.result != SuccessExecutionResult()) {
    notify_batch_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    notify_batch_consume_budget_context.Finish();
    return;
  }

  const auto budget_key_time_frames =
      load_budget_key_timeframe_context.response->budget_key_frames;

  // Since all the timeframes are notified together, it is safe to look at
  // the first timeframe's lock to see if the timeframes have been already
  // notified.
  //
  // Ensure that the request has arrived with the right transaction id. If the
  // id is different there is a chance that the current timeframe and also the
  // rest of the timeframes was notified already and this is a retry
  // operation.
  if (budget_key_time_frames[0]->active_transaction_id.load() !=
      notify_batch_consume_budget_context.request->transaction_id) {
    notify_batch_consume_budget_context.result = SuccessExecutionResult();
    notify_batch_consume_budget_context.Finish();
    return;
  }

  vector<BudgetKeyTimeframeUpdateInfo> timeframes_to_update;
  timeframes_to_update.reserve(budget_key_time_frames.size());
  for (unsigned int i = 0; i < budget_key_time_frames.size(); i++) {
    // Release lock and Swap active token value with the actual token.
    timeframes_to_update.emplace_back();
    timeframes_to_update.back().active_token_count = 0;
    timeframes_to_update.back().active_transaction_id.high = 0;
    timeframes_to_update.back().active_transaction_id.low = 0;
    timeframes_to_update.back().token_count =
        budget_key_time_frames[i]->active_token_count;
    timeframes_to_update.back().reporting_time =
        notify_batch_consume_budget_context.request->time_buckets[i];
  }

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.activity_id =
      notify_batch_consume_budget_context.activity_id;
  update_budget_key_timeframe_context.correlation_id =
      notify_batch_consume_budget_context.correlation_id;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update =
      timeframes_to_update;
  update_budget_key_timeframe_context.callback =
      bind(&BatchConsumeBudgetTransactionProtocol::OnNotifyLogged, this,
           notify_batch_consume_budget_context, _1);

  auto execution_result = budget_key_timeframe_manager_->Update(
      update_budget_key_timeframe_context);
  if (execution_result != SuccessExecutionResult()) {
    notify_batch_consume_budget_context.result = execution_result;
    notify_batch_consume_budget_context.Finish();
    return;
  }
}

void BatchConsumeBudgetTransactionProtocol::OnNotifyLogged(
    AsyncContext<NotifyBatchConsumeBudgetRequest,
                 NotifyBatchConsumeBudgetResponse>&
        notify_batch_consume_budget_context,
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context) noexcept {
  notify_batch_consume_budget_context.result =
      update_budget_key_timeframe_context.result;
  notify_batch_consume_budget_context.Finish();
}

ExecutionResult BatchConsumeBudgetTransactionProtocol::Abort(
    AsyncContext<AbortBatchConsumeBudgetRequest,
                 AbortBatchConsumeBudgetResponse>&
        abort_batch_consume_budget_context) noexcept {
  if (abort_batch_consume_budget_context.request->transaction_id == kZeroUuid) {
    return FailureExecutionResult(
        core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID);
  }

  if (abort_batch_consume_budget_context.request->time_buckets.size() <= 1) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_LESS_BUDGETS_TO_CONSUME);
  }

  LoadBudgetKeyTimeframeRequest request;
  request.reporting_times =
      abort_batch_consume_budget_context.request->time_buckets;

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(move(request)),
          bind(&BatchConsumeBudgetTransactionProtocol::OnAbortBudgetKeyLoaded,
               this, abort_batch_consume_budget_context, _1),
          abort_batch_consume_budget_context.activity_id);

  return budget_key_timeframe_manager_->Load(load_budget_key_timeframe_context);
}

void BatchConsumeBudgetTransactionProtocol::OnAbortBudgetKeyLoaded(
    AsyncContext<AbortBatchConsumeBudgetRequest,
                 AbortBatchConsumeBudgetResponse>&
        abort_batch_consume_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // If there is an error return
  if (load_budget_key_timeframe_context.result != SuccessExecutionResult()) {
    abort_batch_consume_budget_context.result =
        load_budget_key_timeframe_context.result;
    abort_batch_consume_budget_context.Finish();
    return;
  }

  const auto budget_key_time_frames =
      load_budget_key_timeframe_context.response->budget_key_frames;

  // TODO: Keeping this the same as
  // ConsumeBudgetTransactionProtocol's. This needs to be revisited.
  //
  // Ensure that the request has arrived with the right transaction id. If the
  // id is different there is a chance that the current timeframe and also the
  // rest of the timeframes was notified already and this is a retry
  // operation.
  if (budget_key_time_frames[0]->active_transaction_id.load() !=
      abort_batch_consume_budget_context.request->transaction_id) {
    abort_batch_consume_budget_context.result = SuccessExecutionResult();
    abort_batch_consume_budget_context.Finish();
    return;
  }

  vector<BudgetKeyTimeframeUpdateInfo> timeframes_to_update;
  for (unsigned int i = 0; i < budget_key_time_frames.size(); i++) {
    // Release lock and restore original token.
    timeframes_to_update.emplace_back();
    timeframes_to_update.back().active_token_count = 0;
    timeframes_to_update.back().active_transaction_id.high = 0;
    timeframes_to_update.back().active_transaction_id.low = 0;
    timeframes_to_update.back().token_count =
        budget_key_time_frames[i]->token_count;
    timeframes_to_update.back().reporting_time =
        abort_batch_consume_budget_context.request->time_buckets[i];
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
  update_budget_key_timeframe_context.request->timeframes_to_update =
      std::move(timeframes_to_update);
  update_budget_key_timeframe_context.callback =
      bind(&BatchConsumeBudgetTransactionProtocol::OnAbortLogged, this,
           abort_batch_consume_budget_context, _1);

  auto execution_result = budget_key_timeframe_manager_->Update(
      update_budget_key_timeframe_context);
  if (execution_result != SuccessExecutionResult()) {
    abort_batch_consume_budget_context.result = execution_result;
    abort_batch_consume_budget_context.Finish();
    return;
  }
}

void BatchConsumeBudgetTransactionProtocol::OnAbortLogged(
    AsyncContext<AbortBatchConsumeBudgetRequest,
                 AbortBatchConsumeBudgetResponse>&
        abort_batch_consume_budget_context,
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context) noexcept {
  abort_batch_consume_budget_context.result =
      update_budget_key_timeframe_context.result;
  abort_batch_consume_budget_context.Finish();
}

}  // namespace google::scp::pbs
