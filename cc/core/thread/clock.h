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

#ifndef CC_CORE_THREAD_CLOCK_H_
#define CC_CORE_THREAD_CLOCK_H_

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace privacy_sandbox::pbs_common::internal {

// Provides an interface for time-related operations, allowing to test with
// simulated time. This base class uses the real system clock and standard
// Abseil time functions.
class Clock {
 public:
  Clock() = default;
  virtual ~Clock() = default;

  // Returns the current time.
  // By default, returns the real system time using absl::Now().
  virtual absl::Time Now() { return absl::Now(); }

  // Waits until the specified deadline or until the notification is triggered.
  // By default, uses absl::Notification::WaitForNotificationWithDeadline.
  //
  // Args:
  //   deadline: The absolute time to wait until.
  //   notification: absl::Notification to wait on.
  // Returns:
  //   True if the notification was triggered before the deadline, false
  //   otherwise (deadline reached).
  virtual bool WaitForNotificationWithDeadline(
      const absl::Time& deadline, const absl::Notification& notification) {
    return notification.WaitForNotificationWithDeadline(deadline);
  }

  // Waits for the specified duration or until the notification is triggered.
  // By default, uses absl::Notification::WaitForNotificationWithTimeout.
  //
  // Args:
  //   timeout: The maximum duration to wait.
  //   notification: absl::Notification to wait on.
  // Returns:
  //   True if the notification was triggered before the timeout, false
  //   otherwise (timeout expired).
  virtual bool WaitForNotificationWithTimeout(
      const absl::Duration& timeout, const absl::Notification& notification) {
    return notification.WaitForNotificationWithTimeout(timeout);
  }

  // Pauses the current thread for the specified duration.
  // By default, uses absl::SleepFor.
  //
  // Args:
  //   duration: The amount of time to sleep.
  virtual void SleepFor(const absl::Duration& duration) {
    absl::SleepFor(duration);
  }
};

// A simulated clock implementation for testing purposes.
// This clock does not rely on the system clock. Instead, it maintains its own
// internal time (`current_`) which starts at the time of construction.
// Time only advances when `SleepFor`, `WaitForNotificationWithDeadline`, or
// `WaitForNotificationWithTimeout` are called.
//
// This class is thread-safe.
class SimulatedClock : public Clock {
 public:
  // Default constructor. Initializes the internal time to the real time
  // at the moment of construction using absl::Now().
  SimulatedClock() = default;
  ~SimulatedClock() override = default;

  // Returns the current simulated time. This does not advance time.
  absl::Time Now() override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return current_;
  }

  // Simulates waiting until a deadline or notification.
  // If the notification has not been triggered and the current simulated time
  // is before the deadline, advances the simulated time to the deadline.
  // Otherwise, the simulated time remains unchanged.
  //
  // Args:
  //   deadline: The absolute simulated time to "wait" until.
  //   notification: absl::Notification to check.
  // Returns:
  //   True if the notification had already been triggered when called, false
  //   otherwise. Note that this differs from the base class behavior; it does
  //   not actually wait but checks the state and advances time if needed.
  bool WaitForNotificationWithDeadline(const absl::Time& deadline,
                                       const absl::Notification& notification)
      override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    // Only advance time if the notification hasn't happened and the deadline is
    // in the future relative to the current simulated time.
    if (!notification.HasBeenNotified() && current_ < deadline) {
      current_ = deadline;
    }
    return notification.HasBeenNotified();
  }

  // Simulates waiting for a duration or notification.
  // If the notification has not been triggered when this function is called,
  // advances the simulated time by the specified `duration`. Otherwise, the
  // simulated time remains unchanged.
  //
  // Args:
  //   duration: The maximum simulated duration to "wait".
  //   notification: The notification object to check.
  // Returns:
  //   True if the notification had already been triggered when called, false
  //   otherwise. Note that this differs from the base class behavior; it does
  //   not actually wait but checks the state and advances time if needed.
  bool WaitForNotificationWithTimeout(const absl::Duration& duration,
                                      const absl::Notification& notification)
      override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    if (!notification.HasBeenNotified()) {
      current_ += duration;
    }
    return notification.HasBeenNotified();
  }

  // Simulates sleeping by advancing the internal simulated time.
  //
  // Args:
  //   duration: The amount of simulated time to advance.
  void SleepFor(const absl::Duration& duration) override
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    current_ += duration;
  }

 private:
  absl::Time current_ ABSL_GUARDED_BY(mutex_) = absl::Now();
  mutable absl::Mutex mutex_;
};

}  // namespace privacy_sandbox::pbs_common::internal

#endif  // CC_CORE_THREAD_CLOCK_H_
