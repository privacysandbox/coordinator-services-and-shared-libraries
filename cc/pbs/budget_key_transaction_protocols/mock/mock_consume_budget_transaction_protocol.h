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
class MockConsumeBudgetTransactionProtocol
    : public ConsumeBudgetTransactionProtocolInterface {
 public:
  MockConsumeBudgetTransactionProtocol() {}

  std::function<core::ExecutionResult(
      core::AsyncContext<PrepareConsumeBudgetRequest,
                         PrepareConsumeBudgetResponse>&)>
      prepare_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&)>
      commit_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&)>
      notify_mock;

  std::function<core::ExecutionResult(
      core::AsyncContext<AbortConsumeBudgetRequest,
                         AbortConsumeBudgetResponse>&)>
      abort_mock;

  core::ExecutionResult Prepare(
      core::AsyncContext<PrepareConsumeBudgetRequest,
                         PrepareConsumeBudgetResponse>&
          prepare_consume_budget_context) noexcept override {
    if (prepare_mock) {
      return prepare_mock(prepare_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Commit(
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
          commit_consume_budget_context) noexcept override {
    if (commit_mock) {
      return commit_mock(commit_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Notify(
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
          notify_consume_budget_context) noexcept override {
    if (notify_mock) {
      return notify_mock(notify_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Abort(
      core::AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
          abort_consume_budget_context) noexcept override {
    if (abort_mock) {
      return abort_mock(abort_consume_budget_context);
    }
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs::budget_key::mock
