// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "core/interface/async_executor_interface.h"

namespace google::scp::pbs {
// NOTE: Added this for LeasableLock's Sync use case and this must not be used
// elsewhere. This is swapped instead of an AsyncExecutor to get
// Sync behavior on NoSQLDatabaseProvider for LeasableLock. Once we have support
// for Sync API on NoSQLDatabaseProvider, this will be removed.
// TODO: b/279493757 Add Sync API for
// GCPSpanner/Dynamo/NoSQLDatabaseProviderInterface
//
/**
 * @brief Synchronous blocking Executor. Any task scheduled on this would
 * execute on the scheduling (calling) thread itself, i.e. no async operation is
 * performed and no thread pool is required.
 */
class SynchronousExecutor : public core::AsyncExecutorInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Schedule(const core::AsyncOperation& work,
                                 core::AsyncPriority) noexcept override {
    work();
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Schedule(
      const core::AsyncOperation& work, core::AsyncPriority priority,
      core::AsyncExecutorAffinitySetting) noexcept override {
    work();
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult ScheduleFor(const core::AsyncOperation& work,
                                    core::Timestamp) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult ScheduleFor(
      const core::AsyncOperation& work, core::Timestamp,
      core::AsyncExecutorAffinitySetting) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult ScheduleFor(
      const core::AsyncOperation& work, core::Timestamp,
      core::TaskCancellationLambda&) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult ScheduleFor(
      const core::AsyncOperation& work, core::Timestamp timestamp,
      core::TaskCancellationLambda& cancellation_callback,
      core::AsyncExecutorAffinitySetting affinity) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }
};
}  // namespace google::scp::pbs
