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

#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/transaction_protocol_interface.h"
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"

namespace google::scp::pbs {
/*! @copydoc ConsumeBudgetTransactionProtocolInterface
 */
class ConsumeBudgetTransactionProtocol
    : public ConsumeBudgetTransactionProtocolInterface {
 public:
  ConsumeBudgetTransactionProtocol(
      const std::shared_ptr<BudgetKeyTimeframeManagerInterface>
          budget_key_timeframe_manager)
      : budget_key_timeframe_manager_(budget_key_timeframe_manager) {}

  core::ExecutionResult Prepare(
      core::AsyncContext<PrepareConsumeBudgetRequest,
                         PrepareConsumeBudgetResponse>&
          prepare_consume_budget_context) noexcept override;

  core::ExecutionResult Commit(
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
          commit_consume_budget_context) noexcept override;

  core::ExecutionResult Notify(
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
          notify_consume_budget_context) noexcept override;

  core::ExecutionResult Abort(
      core::AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
          abort_consume_budget_context) noexcept override;

 protected:
  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the prepare phase.
   *
   * @param prepare_consume_budget_context The prepare consume budget
   * operation context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnPrepareBudgetKeyLoaded(
      core::AsyncContext<PrepareConsumeBudgetRequest,
                         PrepareConsumeBudgetResponse>&
          prepare_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the commit phase.
   *
   * @param commit_consume_budget_context The commit consume budget operation
   * context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnCommitBudgetKeyLoaded(
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
          commit_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Is called when the commit operation logging is completed.
   *
   * @param budget_key_time_frame The budge key timeframe for the
   * logging operation.
   * @param commit_consume_budget_context The commit consume budget operation
   * context.
   * @param update_budget_key_timeframe_context The update budget key timeframe
   * operation context.
   */
  void OnCommitLogged(
      std::shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
          commit_consume_budget_context,
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept;
  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the notify phase.
   *
   * @param notify_consume_budget_context The notify consume budget operation
   * context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnNotifyBudgetKeyLoaded(
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
          notify_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Is called when the notify operation logging is completed.
   *
   * @param budget_key_time_frame The budge key timeframe for the
   * logging operation.
   * @param notify_consume_budget_context The notify consume budget operation
   * context.
   * @param update_budget_key_timeframe_context The update budget key timeframe
   * operation context.
   */
  void OnNotifyLogged(
      std::shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
          notify_consume_budget_context,
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept;
  /**
   * @brief Notification method called once the key has been loaded from the
   * data storage for the abort phase.
   *
   * @param abort_consume_budget_context The abort consume budget operation
   * context.
   * @param load_budget_key_timeframe_context The load budget key frame
   * context.
   */
  void OnAbortBudgetKeyLoaded(
      core::AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
          abort_consume_budget_context,
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept;

  /**
   * @brief Is called when the abort operation logging is completed.
   *
   * @param budget_key_time_frame The budge key timeframe for the
   * logging operation.
   * @param abort_consume_budget_context The abort consume budget operation
   * context.
   * @param update_budget_key_timeframe_context The update budget key timeframe
   * operation context.
   */
  void OnAbortLogged(
      std::shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
      core::AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
          abort_consume_budget_context,
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept;

 private:
  const std::shared_ptr<BudgetKeyTimeframeManagerInterface>
      budget_key_timeframe_manager_;
};
}  // namespace google::scp::pbs
