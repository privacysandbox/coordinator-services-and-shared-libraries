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
#include <unordered_map>
#include <vector>

#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/transaction_protocol_interface.h"
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"

namespace google::scp::pbs {
/*! @copydoc BatchConsumeBudgetTransactionProtocolInterface
 */
class BatchConsumeBudgetTransactionProtocol
    : public BatchConsumeBudgetTransactionProtocolInterface {
 public:
  BatchConsumeBudgetTransactionProtocol(
      const std::shared_ptr<BudgetKeyTimeframeManagerInterface>
          budget_key_timeframe_manager)
      : budget_key_timeframe_manager_(budget_key_timeframe_manager) {}

  core::ExecutionResult Prepare(
      core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                         PrepareBatchConsumeBudgetResponse>&
          prepare_batch_consume_budget_context) noexcept override;

  core::ExecutionResult Commit(
      core::AsyncContext<CommitBatchConsumeBudgetRequest,
                         CommitBatchConsumeBudgetResponse>&
          commit_batch_consume_budget_context) noexcept override;

  core::ExecutionResult Notify(
      core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                         NotifyBatchConsumeBudgetResponse>&
          notify_batch_consume_budget_context) noexcept override;

  core::ExecutionResult Abort(
      core::AsyncContext<AbortBatchConsumeBudgetRequest,
                         AbortBatchConsumeBudgetResponse>&
          abort_batch_consume_budget_context) noexcept override;

 protected:
  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the prepare phase.
   *
   * @param prepare_batch_consume_budget_context The prepare batch consume
   * budget operation context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnPrepareBudgetKeyLoaded(
      core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                         PrepareBatchConsumeBudgetResponse>&
          prepare_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the commit phase.
   *
   * @param commit_batch_consume_budget_context The commit batch consume budget
   * operation context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnCommitBudgetKeyLoaded(
      core::AsyncContext<CommitBatchConsumeBudgetRequest,
                         CommitBatchConsumeBudgetResponse>&
          commit_batch_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Is called when the commit operation logging is completed.
   *
   * @param commit_batch_consume_budget_context The commit batch consume budget
   * operation context.
   * @param update_budget_key_timeframe_context The update budget key timeframe
   * operation context.
   */
  void OnCommitLogged(core::AsyncContext<CommitBatchConsumeBudgetRequest,
                                         CommitBatchConsumeBudgetResponse>&
                          commit_batch_consume_budget_context,
                      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                                         UpdateBudgetKeyTimeframeResponse>&
                          update_budget_key_timeframe_context) noexcept;

  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the notify phase.
   *
   * @param notify_batch_consume_budget_context The notify batch consume budget
   * operation context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnNotifyBudgetKeyLoaded(
      core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                         NotifyBatchConsumeBudgetResponse>&
          notify_batch_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Is called when the notify operation logging is completed.
   *
   * @param notify_batch_consume_budget_context The notify batch consume budget
   * operation context.
   * @param update_budget_key_timeframe_context The update budget key timeframe
   * operation context.
   */
  void OnNotifyLogged(core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                                         NotifyBatchConsumeBudgetResponse>&
                          notify_batch_consume_budget_context,
                      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                                         UpdateBudgetKeyTimeframeResponse>&
                          update_budget_key_timeframe_context) noexcept;

  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the abort phase.
   *
   * @param abort_batch_consume_budget_context The abort batch consume budget
   * operation context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnAbortBudgetKeyLoaded(
      core::AsyncContext<AbortBatchConsumeBudgetRequest,
                         AbortBatchConsumeBudgetResponse>&
          abort_batch_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Is called when the abort operation logging is completed.
   *
   * @param abort_batch_consume_budget_context The abort batch consume budget
   * operation context.
   * @param update_budget_key_timeframe_context The update budget key timeframe
   * operation context.
   */
  void OnAbortLogged(core::AsyncContext<AbortBatchConsumeBudgetRequest,
                                        AbortBatchConsumeBudgetResponse>&
                         abort_batch_consume_budget_context,
                     core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                                        UpdateBudgetKeyTimeframeResponse>&
                         update_budget_key_timeframe_context) noexcept;

 private:
  const std::shared_ptr<BudgetKeyTimeframeManagerInterface>
      budget_key_timeframe_manager_;
};
}  // namespace google::scp::pbs
