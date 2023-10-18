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

#include "execution_watchdog.h"

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <thread>

#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/type_def.h"
#include "include/v8.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::TimeProvider;
using std::lock_guard;
using std::mutex;
using std::thread;
using std::unique_lock;
using std::chrono::nanoseconds;

static constexpr int kMsToNsConversionBase = 1e6;

namespace google::scp::roma::worker {

ExecutionResult ExecutionWatchDog::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult ExecutionWatchDog::Run() noexcept {
  is_stop_called_.store(false);
  execution_watchdog_thread_ = thread(&ExecutionWatchDog::WaitForTimeout, this);
  return SuccessExecutionResult();
}

ExecutionResult ExecutionWatchDog::Stop() noexcept {
  if (is_stop_called_.load()) {
    return SuccessExecutionResult();
  }

  is_stop_called_.store(true);
  condition_variable_.notify_one();
  if (execution_watchdog_thread_.joinable()) {
    execution_watchdog_thread_.join();
  }

  return SuccessExecutionResult();
}

bool ExecutionWatchDog::IsTerminateCalled() noexcept {
  return is_terminate_called_.load();
}

void ExecutionWatchDog::StartTimer(
    v8::Isolate* isolate, core::TimeDuration ms_before_timeout) noexcept {
  {
    lock_guard lock(mutex_);
    // Cancel TerminateExecution in case there was a previous
    // isolate->TerminateExecution() flag alive
    isolate->CancelTerminateExecution();
    v8_isolate_ = isolate;
    is_terminate_called_.store(false);
    nanoseconds_before_timeout_ = ms_before_timeout * kMsToNsConversionBase;
    timeout_timestamp_ =
        TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks() +
        nanoseconds_before_timeout_;
  }
  condition_variable_.notify_one();
}

void ExecutionWatchDog::EndTimer() noexcept {
  lock_guard lock(mutex_);
  nanoseconds_before_timeout_ = UINT64_MAX;
  timeout_timestamp_ = UINT64_MAX;
}

void ExecutionWatchDog::WaitForTimeout() noexcept {
  unique_lock<mutex> thread_lock(mutex_);
  while (!is_stop_called_) {
    auto current_timestamp =
        TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
    if (current_timestamp > timeout_timestamp_) {
      v8_isolate_->TerminateExecution();
      is_terminate_called_.store(true);
      timeout_timestamp_ = UINT64_MAX;
    }

    condition_variable_.wait_for(thread_lock,
                                 nanoseconds(nanoseconds_before_timeout_));
  }
}

}  // namespace google::scp::roma::worker
