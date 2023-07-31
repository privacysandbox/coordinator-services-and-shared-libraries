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

#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {
/// Defines operation type.
typedef std::function<void()> AsyncOperation;

/// Async operation execution priority.
enum class AsyncPriority {
  /**
   * @brief Will be scheduled when all the previous operations have finished and
   * a thread is available. This type is suitable for the incoming requests into
   * the system. To ensure operations are executed serially and fairly.
   */
  Normal = 0,
  /**
   * @brief Higher priority than the normal operations. But no guarantee to be
   * executed as fast as Urgent. This type is suitable for the callbacks.
   */
  High = 1,
  /**
   * @brief Will be executed immediately as soon as a thread is available. This
   * type is suitable for operations that need to be scheduled at a certain time
   * or run as fast as possible. Such as garbage collection, or retry
   * operations.
   */
  Urgent = 2,
};

/**
 * @brief AsyncExecutor is the main thread-pool of the service. It controls the
 * number of threads that are used across the application and is capable of
 * scheduling tasks with different priorities.
 */
class AsyncExecutorInterface : public ServiceInterface {
 public:
  virtual ~AsyncExecutorInterface() = default;

  /**
   * @brief Schedules a task with certain priority to be execute immediately or
   * deferred.
   * @param work the task that needs to be scheduled.
   * @param priority the priority of the task.
   * @return ExecutionResult result of the execution with possible error code.
   */
  virtual ExecutionResult Schedule(const AsyncOperation& work,
                                   AsyncPriority priority) noexcept = 0;

  /**
   * @brief Schedules a task to be executed after the specified time.
   * NOTE: There is no guarantee in terms of execution of the task at the
   * time specified.
   *
   * @param work the task that needs to be scheduled.
   * @param timestamp the timestamp to the task to be executed.
   * @return ExecutionResult result of the execution with possible error code.
   */
  virtual ExecutionResult ScheduleFor(const AsyncOperation& work,
                                      Timestamp timestamp) noexcept = 0;

  /**
   * @brief Schedules a task to be executed after the specified
   * time. Cancellation callback is provided for the user to cancel the task if
   * neccessary.
   *
   * @param work the task that needs to be scheduled.
   * @param timestamp the timestamp to the task to be executed.
   * @param cancellation_callback the cancellation callback to cancel the work.
   * @return ExecutionResult result of the execution with possible error code.
   */
  virtual ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      std::function<bool()>& cancellation_callback) noexcept = 0;
};
}  // namespace google::scp::core
