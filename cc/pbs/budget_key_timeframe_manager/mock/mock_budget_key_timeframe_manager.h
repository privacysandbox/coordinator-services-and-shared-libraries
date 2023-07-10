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

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::buget_key_timeframe_manager::mock {
/*! @copydoc BudgetKeyTimeframeManagerInterface
 */
class MockBudgetKeyTimeframeManager
    : public BudgetKeyTimeframeManagerInterface {
 public:
  MockBudgetKeyTimeframeManager() { id = core::common::Uuid::GenerateUuid(); }

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult CanUnload() noexcept override {
    if (can_unload_mock) {
      return can_unload_mock();
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Load(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept override {
    return load_function(load_budget_key_timeframe_context);
  }

  core::ExecutionResult Update(
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept override {
    return update_function(update_budget_key_timeframe_context);
  }

  const core::common::Uuid GetId() noexcept { return id; }

  core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>&
          checkpoint_logs) noexcept {
    if (checkpoint_mock) {
      return checkpoint_mock(checkpoint_logs);
    }
    return core::SuccessExecutionResult();
  }

  std::function<core::ExecutionResult(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&)>
      load_function;

  std::function<core::ExecutionResult(
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&)>
      update_function;

  std::function<core::ExecutionResult(
      std::shared_ptr<std::list<core::CheckpointLog>>&)>
      checkpoint_mock;

  std::function<core::ExecutionResult()> can_unload_mock;

  core::common::Uuid id;
};
}  // namespace google::scp::pbs::buget_key_timeframe_manager::mock
