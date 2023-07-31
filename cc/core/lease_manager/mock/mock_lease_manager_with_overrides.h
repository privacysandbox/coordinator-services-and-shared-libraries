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

#include <memory>
#include <string>

#include "core/lease_manager/src/lease_manager.h"

namespace google::scp::core::lease_manager::mock {
class MockLeaseManagerWithOverrides : public LeaseManager {
 public:
  MockLeaseManagerWithOverrides(
      TimeDuration lease_enforcer_frequency_in_milliseconds,
      TimeDuration lease_obtain_max_time_threshold_in_milliseconds)
      : LeaseManager(lease_enforcer_frequency_in_milliseconds,
                     lease_obtain_max_time_threshold_in_milliseconds) {}

  void SetTerminateProcessFunction(
      std::function<void()> terminate_process_function) {
    terminate_process_function_ = terminate_process_function;
  }

  bool IsLeaseEnforcerThreadStarted() {
    return lease_enforcer_thread_started_.load();
  }

  bool IsLeaseObtainerThreadStarted() {
    return lease_obtainer_thread_started_.load();
  }
};
}  // namespace google::scp::core::lease_manager::mock
