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
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

#include "core/interface/service_interface.h"
#include "core/interface/type_def.h"
#include "include/v8.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::roma::worker {
/**
 * @brief ExecutionWatchDog starts a thread that used to monitor the execution
 * time of each code object. If the code object execution time is over the
 * limit, ExecutionWatchDog will forcefully terminate v8 isolate.
 *
 */
class ExecutionWatchDog : public core::ServiceInterface {
 public:
  explicit ExecutionWatchDog(v8::Isolate* isolate) : v8_isolate_(isolate) {}

  virtual core::ExecutionResult Init() noexcept;
  virtual core::ExecutionResult Run() noexcept;
  virtual core::ExecutionResult Stop() noexcept;

  /// @brief Start timing execution. This function will reset the
  /// timeout_timestamp_ for each code object execution.
  virtual void StartTimer(core::TimeDuration ms_before_timeout) noexcept;

  /// @brief End timing execution. This function will reset the
  /// timeout_timestamp_ to UINT64_MAX to avoid terminate standby isolate.
  virtual void EndTimer() noexcept;

 protected:
  /// @brief Timer function running in ExecutionWatchDog thread.
  virtual void WaitForTimeout() noexcept;

 private:
  /// @brief An instance of v8 isolate.
  v8::Isolate* v8_isolate_;
  /// @brief Stop signal of ExecutionWatchDog.
  std::atomic<bool> is_stop_called_;

  /// @brief Execution time limit for one script.
  core::TimeDuration nanoseconds_before_timeout_{UINT64_MAX};
  /// @brief The timestamp of the execution timeout.
  core::Timestamp timeout_timestamp_{UINT64_MAX};

  /// @brief Used in combination with the condition variable for signaling the
  /// thread that Stop() or StartTimer() is called.
  std::mutex mutex_;
  /// @brief Used in combination with the mutex for signaling the thread that
  /// Stop() or StartTimer() is called.
  std::condition_variable condition_variable_;

  /// @brief ExecutionWatchDog thread.
  std::thread execution_watchdog_thread_;
};

}  // namespace google::scp::roma::worker
