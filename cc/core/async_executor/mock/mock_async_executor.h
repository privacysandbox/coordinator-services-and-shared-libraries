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

#include "cc/core/interface/async_executor_interface.h"

namespace privacy_sandbox::pbs_common {
class MockAsyncExecutor : public AsyncExecutorInterface {
 public:
  MockAsyncExecutor() {}

  google::scp::core::ExecutionResult Init() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Run() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Stop() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Schedule(
      const AsyncOperation& work, AsyncPriority priority) noexcept override {
    if (schedule_mock) {
      return schedule_mock(work);
    }

    work();
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Schedule(
      const AsyncOperation& work, AsyncPriority priority,
      AsyncExecutorAffinitySetting) noexcept override {
    return Schedule(work, priority);
  }

  google::scp::core::ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp) noexcept override {
    if (schedule_for_mock) {
      std::function<bool()> callback;
      return schedule_for_mock(work, timestamp, callback);
    }

    work();
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      AsyncExecutorAffinitySetting) noexcept override {
    return ScheduleFor(work, timestamp);
  }

  google::scp::core::ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      std::function<bool()>& cancellation_callback) noexcept override {
    if (schedule_for_mock) {
      return schedule_for_mock(work, timestamp, cancellation_callback);
    }

    work();
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      std::function<bool()>& cancellation_callback,
      AsyncExecutorAffinitySetting) noexcept override {
    return ScheduleFor(work, timestamp, cancellation_callback);
  }

  std::function<google::scp::core::ExecutionResult(
      const privacy_sandbox::pbs_common::AsyncOperation& work)>
      schedule_mock;
  std::function<google::scp::core::ExecutionResult(
      const privacy_sandbox::pbs_common::AsyncOperation& work,
      privacy_sandbox::pbs_common::Timestamp, std::function<bool()>&)>
      schedule_for_mock;
};
}  // namespace privacy_sandbox::pbs_common
