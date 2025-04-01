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
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace google::scp::core {

PeriodicClosure::PeriodicClosure(absl::Duration interval,
                                 absl::AnyInvocable<void()> closure)
    : interval_(interval), closure_(std::move(closure)) {}

PeriodicClosure::~PeriodicClosure() {
  Stop();
}

absl::Status PeriodicClosure::StartNow() {
  return StartInternal(/*run_first=*/true);
}

absl::Status PeriodicClosure::StartDelayed() {
  return StartInternal(/*run_first=*/false);
}

void PeriodicClosure::Stop() {
  if (thread_ != nullptr && !notification_.HasBeenNotified()) {
    notification_.Notify();
    thread_ = nullptr;
  }
}

bool PeriodicClosure::IsRunning() const {
  return thread_ != nullptr && thread_->joinable();
}

absl::Status PeriodicClosure::StartInternal(bool run_first) {
  if (IsRunning()) {
    return absl::FailedPreconditionError("Already running.");
  }
  if (notification_.HasBeenNotified()) {
    return absl::FailedPreconditionError("Already ran.");
  }
  thread_ = std::make_unique<std::jthread>(
      [this, run_first]() mutable {
        if (run_first) {
          closure_();
        }
        while (!notification_.WaitForNotificationWithTimeout(interval_)) {
          closure_();
        }
      });
  return absl::OkStatus();
}

}  // namespace google::scp::core
