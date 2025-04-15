// Copyright 2025 Google LLC
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

#include "cc/core/thread/periodic_closure.h"

#include <memory>
#include <thread>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace privacy_sandbox::pbs_common {
namespace {

// Calculates the next run time that is after the given time.
// The next run time will be the earliest multiple of interval after the
// given time.
//
// Args:
//   after_time: The time after which the next run should be scheduled.
//   last_scheduled: The time when the last execution was scheduled.
//   interval: The interval between executions.
// Returns:
//   The next run time that is a multiple of interval after the given time.
absl::Time CalculateNextRunTime(absl::Time after_time,
                                absl::Time last_scheduled,
                                absl::Duration interval) {
  // Calculate how many intervals have passed since the last scheduled time.
  int64_t intervals_passed =
      absl::ToInt64Nanoseconds(after_time - last_scheduled) /
      absl::ToInt64Nanoseconds(interval);
  // Add one interval to get the next run time.
  return last_scheduled + (intervals_passed + 1) * interval;
}

}  // namespace

PeriodicClosure::PeriodicClosure(absl::Duration interval,
                                 absl::AnyInvocable<void()> closure,
                                 absl::Duration startup_delay)
    : interval_(interval),
      startup_delay_(startup_delay),
      closure_(std::move(closure)) {
  clock_ = std::make_shared<internal::SimulatedClock>();
}

PeriodicClosure::PeriodicClosure(absl::Duration interval,
                                 absl::AnyInvocable<void()> closure,
                                 std::shared_ptr<internal::Clock> clock,
                                 absl::Duration startup_delay)
    : interval_(interval),
      startup_delay_(startup_delay),
      closure_(std::move(closure)),
      clock_(clock) {}

PeriodicClosure::~PeriodicClosure() {
  Stop();
}

absl::Status PeriodicClosure::Start() {
  absl::MutexLock lock(&mutex_);
  // Prevent starting if already running or if we've already run once.
  if (thread_ != nullptr && thread_->joinable()) {
    return absl::FailedPreconditionError("Already running.");
  }
  if (notification_.HasBeenNotified()) {
    return absl::FailedPreconditionError("Already ran.");
  }
  return StartInternal();
}

void PeriodicClosure::Stop() {
  absl::MutexLock lock(&mutex_);
  // Only stop if we have a running thread and haven't already notified it to
  // stop.
  if (thread_ != nullptr && !notification_.HasBeenNotified()) {
    notification_.Notify();
    thread_.reset();
  }
}

bool PeriodicClosure::IsRunning() const {
  absl::MutexLock lock(&mutex_);
  // Thread is running if it exists and is joinable (not detached).
  return thread_ != nullptr && thread_->joinable();
}

absl::Status PeriodicClosure::StartInternal() {
  // Create and start the background thread.
  thread_ = std::make_unique<std::jthread>([this]() mutable {
    absl::Time next_run = clock_->Now() + startup_delay_;

    // Wait until the first execution time or until stopped.
    if (clock_->WaitForNotificationWithDeadline(next_run, notification_)) {
      return;
    }

    // First execution.
    closure_();
    next_run = CalculateNextRunTime(clock_->Now(), next_run, interval_);

    // Main execution loop.
    while (!clock_->WaitForNotificationWithDeadline(next_run, notification_)) {
      closure_();
      next_run = CalculateNextRunTime(clock_->Now(), next_run, interval_);
    }
  });

  return absl::OkStatus();
}

}  // namespace privacy_sandbox::pbs_common
