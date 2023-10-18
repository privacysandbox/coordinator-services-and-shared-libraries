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
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"

namespace google::scp::core::common {
/**
 * @brief Represents the task's current state machine.
 *
 * NotStarted     --> Executing
 * Executing      --> Completed
 *
 * Task can be cancelled only if pre-condition state is 'NotStarted', and moves
 * to 'Cancelled'
 *
 * Initial state: NotStarted
 * Terminal states: Completed or Cancelled
 */
enum class TaskState { NotStarted, Executing, Cancelled, Completed };

using TaskLambda = std::function<void()>;

/**
 * @brief This is a one-off execution task that executes on a newly spun
 * std::thread. This structure holds an std::thread, so care must be ensured
 * that the thread is joined before destructing the object.
 */
class CancellableThreadTask {
 public:
  CancellableThreadTask(
      TaskLambda task_lambda,
      std::chrono::nanoseconds startup_delay = std::chrono::nanoseconds(0))
      : task_lambda_(std::move(task_lambda)),
        task_state_(TaskState::NotStarted),
        start_steady_timestamp_(
            TimeProvider::GetSteadyTimestampInNanoseconds() +
            std::chrono::steady_clock::duration(startup_delay)),
        thread_(std::bind(&CancellableThreadTask::ThreadFunction, this)) {}

  ~CancellableThreadTask() {
    assert(IsCompleted() || IsCancelled());
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  /**
   * @brief Is the task cancelled.
   *
   * @return true
   * @return false
   */
  bool IsCancelled() const;

  /**
   * @brief Cancel the task if it is in cancellable state. If the task indeed is
   * cancelled, then returns true, else returns false.
   */
  bool Cancel();

  /**
   * @brief If the task completed execution.
   *
   * @return true
   * @return false
   */
  bool IsCompleted() const;

  /**
   * @brief If the task is cancellable i.e. a call to Cancel() after this will
   * likely return true (but not guaranteed)
   *
   * @return true
   * @return false
   */
  bool IsCancellable() const;

 protected:
  /// @brief Internal function for thread
  void ThreadFunction();

  /// @brief lambda that will be executed by thread_
  TaskLambda task_lambda_;
  /// @brief upon task execution, this barrier will be changed from 'false' -->
  /// 'true'. If the task needs to be cancelled, the same 'false' --> 'true' is
  /// done.
  std::atomic<TaskState> task_state_;
  /// @brief Task will start at this time.
  std::chrono::nanoseconds start_steady_timestamp_;
  /// @brief thread to execute the task
  std::thread thread_;
};
}  // namespace google::scp::core::common
