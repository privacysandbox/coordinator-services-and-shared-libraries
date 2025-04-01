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

#include "absl/functional/any_invocable.h"
#include "absl/status/status_matchers.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace google::scp::core {
namespace {

using ::absl_testing::IsOk;
using ::google::scp::core::PeriodicClosure;
using ::testing::Not;

TEST(PeriodicClosureTest, IsNotRunning) {
  auto periodic_closure = PeriodicClosure(absl::Milliseconds(1), []() {});
  ASSERT_FALSE(periodic_closure.IsRunning());
}

TEST(PeriodicClosureTest, IsRunning) {
  auto periodic_closure = PeriodicClosure(absl::Milliseconds(1), []() {});
  ASSERT_THAT(periodic_closure.StartNow(), IsOk());
  ASSERT_TRUE(periodic_closure.IsRunning());
}

TEST(PeriodicClosureTest, StartDelayed) {
  absl::Notification notification;
  const absl::Time start = absl::Now();
  absl::Time delayed_start;
  constexpr absl::Duration delay = absl::Milliseconds(2);
  absl::AnyInvocable<void()> callback = [&delayed_start, &notification]() {
    if (!notification.HasBeenNotified()) {
      delayed_start = absl::Now();
      notification.Notify();
    }
  };
  auto periodic_closure = PeriodicClosure(delay, std::move(callback));
  ASSERT_THAT(periodic_closure.StartDelayed(), IsOk());
  notification.WaitForNotification();
  ASSERT_GE(delayed_start - start, delay);
}

TEST(PeriodicClosureTest, StartNow) {
  absl::Notification notification;
  const absl::Time start = absl::Now();
  absl::Time delayed_start;
  constexpr absl::Duration delay = absl::Minutes(2);
  absl::AnyInvocable<void()> callback = [&delayed_start, &notification]() {
    if (!notification.HasBeenNotified()) {
      delayed_start = absl::Now();
      notification.Notify();
    }
  };
  auto periodic_closure = PeriodicClosure(delay, std::move(callback));
  ASSERT_THAT(periodic_closure.StartNow(), IsOk());
  notification.WaitForNotification();
  ASSERT_LT(delayed_start - start, delay);
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
  ASSERT_THAT(periodic_closure.StartNow(), IsOk());
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
  ASSERT_THAT(periodic_closure.StartNow(), IsOk());
  notification.WaitForNotification();
  ASSERT_THAT(periodic_closure.StartNow(), Not(IsOk()));
}

TEST(PeriodicClosureTest, StartAfterStopped) {
  auto periodic_closure = PeriodicClosure(absl::Milliseconds(1), []() {});
  ASSERT_THAT(periodic_closure.StartNow(), IsOk());
  periodic_closure.Stop();
  ASSERT_THAT(periodic_closure.StartNow(), Not(IsOk()));
}

}  // namespace
}  // namespace google::scp::core
