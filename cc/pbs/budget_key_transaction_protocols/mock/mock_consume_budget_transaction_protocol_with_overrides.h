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

#include <functional>
#include <memory>
#include <unordered_map>

#include "core/interface/transaction_protocol_interface.h"
#include "pbs/budget_key_transaction_protocols/src/consume_budget_transaction_protocol.h"
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"

namespace google::scp::pbs::budget_key::mock {

class MockConsumeBudgetTransactionProtocolWithOverrides
    : public ConsumeBudgetTransactionProtocol {
 public:
  MockConsumeBudgetTransactionProtocolWithOverrides(
      const std::shared_ptr<BudgetKeyTimeframeManagerInterface>
          budget_key_timeframe_manager)
      : ConsumeBudgetTransactionProtocol(budget_key_timeframe_manager) {}

  void OnCommitLogged(
      std::shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
          commit_consume_budget_context,
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept {
    ConsumeBudgetTransactionProtocol::OnCommitLogged(
        budget_key_time_frame, commit_consume_budget_context,
        update_budget_key_timeframe_context);
  }

  void OnNotifyLogged(
      std::shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
          notify_consume_budget_context,
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept {
    ConsumeBudgetTransactionProtocol::OnNotifyLogged(
        budget_key_time_frame, notify_consume_budget_context,
        update_budget_key_timeframe_context);
  }

  void OnAbortLogged(
      std::shared_ptr<BudgetKeyTimeframe>& budget_key_time_frame,
      core::AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
          abort_consume_budget_context,
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept {
    ConsumeBudgetTransactionProtocol::OnAbortLogged(
        budget_key_time_frame, abort_consume_budget_context,
        update_budget_key_timeframe_context);
  }
};
}  // namespace google::scp::pbs::budget_key::mock
