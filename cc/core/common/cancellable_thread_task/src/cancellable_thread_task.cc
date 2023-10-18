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

#include "cancellable_thread_task.h"

using std::chrono::milliseconds;
using std::this_thread::sleep_for;

static constexpr milliseconds kStartupDelayWaitLoopIntervalInMilliseconds =
    milliseconds(500);

namespace google::scp::core::common {

bool CancellableThreadTask::IsCancelled() const {
  return task_state_.load() == TaskState::Cancelled;
}

bool CancellableThreadTask::Cancel() {
  TaskState current_state = TaskState::NotStarted;
  return task_state_.compare_exchange_strong(current_state,
                                             TaskState::Cancelled);
}

bool CancellableThreadTask::IsCompleted() const {
  return task_state_.load() == TaskState::Completed;
}

bool CancellableThreadTask::IsCancellable() const {
  return task_state_.load() == TaskState::NotStarted;
}

void CancellableThreadTask::ThreadFunction() {
  // Wait for the startup delay
  while (TimeProvider::GetSteadyTimestampInNanoseconds() <
             start_steady_timestamp_ &&
         task_state_ == TaskState::NotStarted) {
    sleep_for(kStartupDelayWaitLoopIntervalInMilliseconds);
  }

  TaskState current_state = TaskState::NotStarted;
  if (task_state_.compare_exchange_strong(current_state,
                                          TaskState::Executing)) {
    task_lambda_();
    // When task is executing, no else can change the task_state, no need of
    // CAS.
    task_state_ = TaskState::Completed;
  }
}

}  // namespace google::scp::core::common
