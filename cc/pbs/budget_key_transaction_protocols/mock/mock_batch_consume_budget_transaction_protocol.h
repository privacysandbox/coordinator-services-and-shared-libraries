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
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"

namespace google::scp::pbs::budget_key::mock {
/*! @copydoc ConsumeBudgetTransactionProtocolInterface
 */
class MockBatchConsumeBudgetTransactionProtocol
    : public BatchConsumeBudgetTransactionProtocolInterface {
 public:
  MockBatchConsumeBudgetTransactionProtocol() {}

  std::function<core::ExecutionResult(
      core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                         PrepareBatchConsumeBudgetResponse>&)>
      prepare_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<CommitBatchConsumeBudgetRequest,
                         CommitBatchConsumeBudgetResponse>&)>
      commit_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                         NotifyBatchConsumeBudgetResponse>&)>
      notify_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<AbortBatchConsumeBudgetRequest,
                         AbortBatchConsumeBudgetResponse>&)>
      abort_mock;

  core::ExecutionResult Prepare(
      core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                         PrepareBatchConsumeBudgetResponse>&
          prepare_consume_budget_context) noexcept override {
    if (prepare_mock) {
      return prepare_mock(prepare_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Commit(
      core::AsyncContext<CommitBatchConsumeBudgetRequest,
                         CommitBatchConsumeBudgetResponse>&
          commit_consume_budget_context) noexcept override {
    if (commit_mock) {
      return commit_mock(commit_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Notify(
      core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                         NotifyBatchConsumeBudgetResponse>&
          notify_consume_budget_context) noexcept override {
    if (notify_mock) {
      return notify_mock(notify_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Abort(
      core::AsyncContext<AbortBatchConsumeBudgetRequest,
                         AbortBatchConsumeBudgetResponse>&
          abort_consume_budget_context) noexcept override {
    if (abort_mock) {
      return abort_mock(abort_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs::budget_key::mock
