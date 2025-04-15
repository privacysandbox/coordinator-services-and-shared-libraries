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

#ifndef CC_CORE_THREAD_PERIODIC_CLOSURE_H_
#define CC_CORE_THREAD_PERIODIC_CLOSURE_H_

#include <memory>
#include <thread>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "cc/core/thread/clock.h"

namespace privacy_sandbox::pbs_common {
// A thread-safe utility class for executing a closure periodically in a
// background thread. The class owns the background thread and manages its
// lifecycle.
//
// Example usage:
//   auto closure = []() { DoSomething(); };
//   PeriodicClosure periodic(absl::Seconds(1), std::move(closure));
//   periodic.Start();  // Starts executing every second
//   // ... do other work ...
//   periodic.Stop();   // Stops execution
//
// Thread Safety:
//   - All public methods are thread-safe.
//   - Multiple threads can safely call Stop() concurrently.
//   - Only one thread can successfully call Start().
//   - The provided closure is executed in a background thread without
//     any mutex protection. If the closure accesses shared state, it is
//     the user's responsibility to protect that state with appropriate
//     synchronization mechanisms.
//
// Timing behavior:
// 1. First execution:
//      - If `startup_delay` > 0: Waits for `startup_delay` before first
//        execution. If `startup_delay` = 0: Executes immediately after
//        `Start()`.
//
// 2. Subsequent executions:
//      - Each execution is scheduled for `interval` time after the previous
//        execution's start time.
//      - If a closure takes longer than `interval` to complete then the next
//        execution is scheduled for the next interval after the closure
//        finishes.
//
// Timing Visualization:
//
// Case 1: interval = 1hr, startup_delay = 10min, closure time = 10min
//
// Time (min):   0        10       20        70       80       130
//               |--------|--------|---------|--------|--------|
//               ^        ^        ^         ^        ^        ^
//               |        |        |         |        |        |
//           Start()   1st run   end      2nd run   end     3rd run
//                              (10min)            (10min)
//
// Each run is scheduled at previous start + interval.
//
// Case 2: interval = 1hr, startup_delay = 10min, closure time = 70min
//
// Time (min):   0        10                      80              130
//               |--------|----------------------|----------------|
//               ^        ^                      ^                ^
//               |        |                      |                |
//           Start()   1st run (70min long)   1st ends         2nd run starts
//
//
//  If closure takes longer than interval, that interval is skipped. Next
//  execution is scheduled for the earliest possible multiple of interval after
//  the last scheduled time.
class PeriodicClosure {
 public:
  // Constructs a PeriodicClosure that will execute the given closure
  // periodically.
  //
  // Args:
  //   interval: Time between executions. Must be positive.
  //   closure: The function to execute periodically. The closure is executed
  //            in a background thread.
  //   startup_delay: Optional delay before the first execution. Must be
  //                  non-negative. Defaults to zero (no delay).
  PeriodicClosure(absl::Duration interval, absl::AnyInvocable<void()> closure,
                  absl::Duration startup_delay = absl::ZeroDuration());
  ~PeriodicClosure();

  // Not copyable or movable.
  PeriodicClosure(const PeriodicClosure&) = delete;
  PeriodicClosure& operator=(const PeriodicClosure&) = delete;
  PeriodicClosure(PeriodicClosure&&) = delete;
  PeriodicClosure& operator=(PeriodicClosure&&) = delete;

  // Starts the periodic execution of the closure.
  //
  // This method blocks until the background thread has been created and
  // started. The first execution will begin after startup_delay (if specified)
  // or immediately if no delay was set. This method is thread-safe.
  //
  // Returns:
  //   - Ok if started successfully.
  //   - FailedPreconditionError if already running or already ran.
  absl::Status Start() ABSL_LOCKS_EXCLUDED(mutex_);

  // Stops the periodic execution of the closure.
  //
  // This method signals the background thread to stop and blocks until it has
  // terminated. If a closure is currently running, it will be allowed to finish
  // before the thread exits. This method will always return success even if
  // `Stop` has been called before. This method is thread-safe.
  void Stop() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if the closure is currently running. This method is
  // thread-safe.
  bool IsRunning() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Peer class for testing.
  friend class PeriodicClosurePeer;

  // Constructs a PeriodicClosure that will execute the given closure
  // periodically with a custom clock.
  //
  // Args:
  //   interval: Time between executions. Must be positive.
  //   closure: The function to execute periodically. The closure is executed
  //            in a background thread.
  //   startup_delay: Optional delay before the first execution. Must be
  //                  non-negative. Defaults to zero (no delay).
  //   clock: A custom clock that implements the internal::Clock interface.
  PeriodicClosure(absl::Duration interval, absl::AnyInvocable<void()> closure,
                  std::shared_ptr<internal::Clock> clock,
                  absl::Duration startup_delay = absl::ZeroDuration());

  // Internal implementation of Start() that handles the actual thread creation.
  // Requires mutex_ to be held.
  absl::Status StartInternal() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Time between executions.
  absl::Duration interval_;

  // Delay before the first execution.
  absl::Duration startup_delay_;

  // The function to execute periodically.
  absl::AnyInvocable<void()> closure_;

  // The background thread that executes the closure.
  std::unique_ptr<std::jthread> thread_ ABSL_GUARDED_BY(mutex_);

  // Used to signal the thread to stop.
  absl::Notification notification_;

  // Mutex to protect thread state.
  mutable absl::Mutex mutex_;

  // Clock used for timing and sleeping. Allows to use custom clock for testing.
  std::shared_ptr<internal::Clock> clock_;
};

}  // namespace privacy_sandbox::pbs_common

#endif  // CC_CORE_THREAD_PERIODIC_CLOSURE_H_
