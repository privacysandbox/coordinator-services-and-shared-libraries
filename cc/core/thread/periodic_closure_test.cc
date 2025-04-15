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

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status_matchers.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cc/core/thread/clock.h"

namespace privacy_sandbox::pbs_common {

class PeriodicClosurePeer {
 public:
  static PeriodicClosure GetPeriodicClosure(
      absl::Duration interval, absl::AnyInvocable<void()> closure,
      std::shared_ptr<internal::Clock> clock,
      absl::Duration startup_delay = absl::ZeroDuration()) {
    return PeriodicClosure(std::move(interval), std::move(closure),
                           std::move(clock), std::move(startup_delay));
  }
};

namespace {
using ::absl_testing::IsOk;
using ::testing::Not;

TEST(PeriodicClosureTest, IsNotRunning) {
  auto periodic_closure = PeriodicClosure(absl::Milliseconds(1), []() {});
  ASSERT_FALSE(periodic_closure.IsRunning());
}

TEST(PeriodicClosureTest, IsRunning) {
  auto periodic_closure = PeriodicClosure(absl::Milliseconds(1), []() {});
  ASSERT_THAT(periodic_closure.Start(), IsOk());
  ASSERT_TRUE(periodic_closure.IsRunning());
}

TEST(PeriodicClosureTest, StartNow) {
  auto clock = std::make_shared<internal::SimulatedClock>();

  absl::Notification notification;
  const absl::Time start = clock->Now();
  absl::Time first_execution;
  constexpr absl::Duration interval = absl::Minutes(2);
  absl::AnyInvocable<void()> callback = [&first_execution, &notification,
                                         &clock]() {
    if (!notification.HasBeenNotified()) {
      first_execution = clock->Now();
      notification.Notify();
    }
  };

  auto periodic_closure = PeriodicClosurePeer::GetPeriodicClosure(
      interval, std::move(callback), clock);
  ASSERT_THAT(periodic_closure.Start(), IsOk());

  notification.WaitForNotification();
  // Execution should start immediately as no startup delay is set.
  // The first execution happens right after Start() returns.
  ASSERT_EQ(first_execution - start, absl::Duration());
}

TEST(PeriodicClosureTest, StartDelayed) {
  auto clock = std::make_shared<internal::SimulatedClock>();

  absl::Notification notification;
  const absl::Time start = clock->Now();
  absl::Time first_execution;
  constexpr absl::Duration interval = absl::Minutes(2);
  constexpr absl::Duration startup_delay = absl::Milliseconds(10);
  absl::AnyInvocable<void()> callback = [&first_execution, &notification,
                                         &clock]() {
    if (!notification.HasBeenNotified()) {
      first_execution = clock->Now();
      notification.Notify();
    }
  };

  auto periodic_closure = PeriodicClosurePeer::GetPeriodicClosure(
      interval, std::move(callback), clock, startup_delay);
  ASSERT_THAT(periodic_closure.Start(), IsOk());

  notification.WaitForNotification();
  // Execution should start after startup delay.
  ASSERT_EQ(first_execution - start, startup_delay);
}

TEST(PeriodicClosureTest, Stop) {
  absl::Notification notification;
  absl::AnyInvocable<void()> callback = [&notification]() {
    if (!notification.HasBeenNotified()) {
      notification.Notify();
    }
  };
  auto periodic_closure =
      PeriodicClosure(absl::Milliseconds(1), std::move(callback));
  ASSERT_THAT(periodic_closure.Start(), IsOk());
  notification.WaitForNotification();
  periodic_closure.Stop();
  ASSERT_FALSE(periodic_closure.IsRunning());
}

TEST(PeriodicClosureTest, StartAfterStarted) {
  absl::Notification notification;
  absl::AnyInvocable<void()> callback = [&notification]() {
    if (!notification.HasBeenNotified()) {
      notification.Notify();
    }
  };
  auto periodic_closure =
      PeriodicClosure(absl::Milliseconds(1), std::move(callback));
  ASSERT_THAT(periodic_closure.Start(), IsOk());
  notification.WaitForNotification();
  ASSERT_THAT(periodic_closure.Start(), Not(IsOk()));
}

TEST(PeriodicClosureTest, StartAfterStopped) {
  auto periodic_closure = PeriodicClosure(absl::Milliseconds(1), []() {});
  ASSERT_THAT(periodic_closure.Start(), IsOk());
  periodic_closure.Stop();
  ASSERT_THAT(periodic_closure.Start(), Not(IsOk()));
}

TEST(PeriodicClosureTest, StartupDelayAndInterval) {
  auto clock = std::make_shared<internal::SimulatedClock>();

  absl::Notification notification;
  absl::Time first_execution;
  absl::Time second_execution;
  constexpr absl::Duration interval = absl::Milliseconds(100);
  constexpr absl::Duration startup_delay = absl::Milliseconds(10);

  int execution_count = 0;
  absl::AnyInvocable<void()> callback = [&execution_count, &first_execution,
                                         &second_execution, &notification,
                                         &clock]() {
    if (execution_count == 0) {
      first_execution = clock->Now();
    } else if (execution_count == 1) {
      second_execution = clock->Now();
      notification.Notify();
    }
    ++execution_count;
  };

  auto periodic_closure = PeriodicClosurePeer::GetPeriodicClosure(
      interval, std::move(callback), clock, startup_delay);

  const absl::Time start = clock->Now();
  ASSERT_THAT(periodic_closure.Start(), IsOk());
  notification.WaitForNotification();

  // First execution should start after startup_delay.
  const absl::Duration first_delay = first_execution - start;
  ASSERT_EQ(first_delay, startup_delay);

  // Second execution should start after interval from first execution start.
  const absl::Duration execution_interval = second_execution - first_execution;
  ASSERT_EQ(execution_interval, interval);
}

TEST(PeriodicClosureTest, LongRunningClosure) {
  auto clock = std::make_shared<internal::SimulatedClock>();

  absl::Notification notification;
  absl::Time first_execution;
  absl::Time second_execution;
  constexpr absl::Duration interval = absl::Milliseconds(100);

  int execution_count = 0;
  absl::AnyInvocable<void()> callback = [&execution_count, &first_execution,
                                         &second_execution, &notification,
                                         &clock]() {
    if (execution_count == 0) {
      first_execution = clock->Now();
    } else if (execution_count == 1) {
      second_execution = clock->Now();
      notification.Notify();
    }
    ++execution_count;

    constexpr absl::Duration closure_duration_ms =
        absl::Milliseconds(150);  // Closure takes longer than interval.

    // Simulate a long-running closure.
    clock->SleepFor(closure_duration_ms);
  };

  auto periodic_closure = PeriodicClosurePeer::GetPeriodicClosure(
      interval, std::move(callback), clock);

  const absl::Time start = clock->Now();
  ASSERT_THAT(periodic_closure.Start(), IsOk());
  notification.WaitForNotification();

  // First execution should start immediately
  const absl::Duration first_delay = first_execution - start;
  ASSERT_EQ(first_delay, absl::ZeroDuration());

  // Second execution should start after 2*interval from first execution start.
  // This is because the closure takes longer than interval, so the next
  // execution is scheduled for the next interval after the closure finishes.
  const absl::Duration execution_interval = second_execution - first_execution;
  ASSERT_EQ(execution_interval, 2 * interval);
}

TEST(PeriodicClosureTest, ThreadSafety) {
  absl::Notification notification;
  int execution_count = 0;
  absl::Mutex count_mutex;
  int successful_starts = 0;
  absl::Mutex starts_mutex;

  auto periodic_closure = PeriodicClosure(absl::Milliseconds(10),
                                          [&execution_count, &count_mutex]() {
                                            absl::MutexLock lock(&count_mutex);
                                            ++execution_count;
                                          });

  // Spawn multiple threads that try to start/stop/check running state.
  const int kNumThreads = 10;
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(
        [&periodic_closure, &successful_starts, &starts_mutex]() {
          // Try to start multiple times.
          for (int j = 0; j < 10; ++j) {
            if (periodic_closure.Start().ok()) {
              absl::MutexLock lock(&starts_mutex);
              ++successful_starts;
            }
            absl::SleepFor(absl::Milliseconds(1));
          }
        });
  }

  // Wait for all threads to finish.
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify only one start succeeded.
  ASSERT_EQ(successful_starts, 1);
  ASSERT_TRUE(periodic_closure.IsRunning());

  // Spawn threads to stop.
  threads.clear();
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&periodic_closure]() { periodic_closure.Stop(); });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_FALSE(periodic_closure.IsRunning());
}

TEST(PeriodicClosureTest, ConcurrentStartStop) {
  absl::Notification notification;
  int execution_count = 0;
  absl::Mutex count_mutex;
  int successful_starts = 0;
  absl::Mutex starts_mutex;

  auto periodic_closure = PeriodicClosure(absl::Milliseconds(10),
                                          [&execution_count, &count_mutex]() {
                                            absl::MutexLock lock(&count_mutex);
                                            ++execution_count;
                                          });

  // Spawn threads that mix start/stop operations.
  const int kNumThreads = 10;
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(
        [&periodic_closure, i, &successful_starts, &starts_mutex]() {
          for (int j = 0; j < 10; ++j) {
            if (i % 2 == 0) {
              if (periodic_closure.Start().ok()) {
                absl::MutexLock lock(&starts_mutex);
                ++successful_starts;
              }
            } else {
              periodic_closure.Stop();
            }
            absl::SleepFor(absl::Milliseconds(1));
          }
        });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Final state should be consistent.
  ASSERT_FALSE(periodic_closure.IsRunning());
  // At most one start should have succeeded.
  ASSERT_LE(successful_starts, 1);
}

}  // namespace
}  // namespace privacy_sandbox::pbs_common
