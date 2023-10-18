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

#include "core/interface/async_executor_interface.h"

namespace google::scp::core::async_executor::mock {
class MockAsyncExecutor : public core::AsyncExecutorInterface {
 public:
  MockAsyncExecutor() {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Schedule(const AsyncOperation& work,
                           AsyncPriority priority) noexcept override {
    if (schedule_mock) {
      return schedule_mock(work);
    }

    work();
    return SuccessExecutionResult();
  }

  ExecutionResult Schedule(const AsyncOperation& work, AsyncPriority priority,
                           AsyncExecutorAffinitySetting) noexcept override {
    return Schedule(work, priority);
  }

  ExecutionResult ScheduleFor(const AsyncOperation& work,
                              Timestamp timestamp) noexcept override {
    if (schedule_for_mock) {
      std::function<bool()> callback;
      return schedule_for_mock(work, timestamp, callback);
    }

    work();
    return SuccessExecutionResult();
  }

  ExecutionResult ScheduleFor(const AsyncOperation& work, Timestamp timestamp,
                              AsyncExecutorAffinitySetting) noexcept override {
    return ScheduleFor(work, timestamp);
  }

  ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      std::function<bool()>& cancellation_callback) noexcept override {
    if (schedule_for_mock) {
      return schedule_for_mock(work, timestamp, cancellation_callback);
    }

    work();
    return SuccessExecutionResult();
  }

  ExecutionResult ScheduleFor(const AsyncOperation& work, Timestamp timestamp,
                              std::function<bool()>& cancellation_callback,
                              AsyncExecutorAffinitySetting) noexcept override {
    return ScheduleFor(work, timestamp, cancellation_callback);
  }

  std::function<ExecutionResult(const AsyncOperation& work)> schedule_mock;
  std::function<ExecutionResult(const AsyncOperation& work, Timestamp,
                                std::function<bool()>&)>
      schedule_for_mock;
};
}  // namespace google::scp::core::async_executor::mock
