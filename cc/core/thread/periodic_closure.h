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

#include <functional>
#include <memory>
#include <thread>

#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace google::scp::core {

// Runs a closure repeatedly on a thread owned by this class.
// Can only be started once.
// All methods are not thread-safe.
class PeriodicClosure {
 public:
  PeriodicClosure(absl::Duration interval, absl::AnyInvocable<void()> closure);

  ~PeriodicClosure();

  // Not copyable or movable.
  PeriodicClosure(const PeriodicClosure&) = delete;
  PeriodicClosure& operator=(const PeriodicClosure&) = delete;

  // Executes `closure` immediately, then every `interval`.
  absl::Status StartNow();

  // Executes `closure` every `interval`, with no immediate call.
  absl::Status StartDelayed();

  void Stop();

  bool IsRunning() const;

 private:
  absl::Status StartInternal(bool run_first);

  absl::Duration interval_;
  absl::AnyInvocable<void()> closure_;
  std::unique_ptr<std::jthread> thread_;
  absl::Notification notification_;
};

}  // namespace google::scp::core

#endif  // CC_CORE_THREAD_PERIODIC_CLOSURE_H_
