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
#include <optional>
#include <string>

#include "core/interface/lease_manager_interface.h"

namespace google::scp::core::lease_manager::mock {
class MockLeasableLock : public LeasableLockInterface {
 public:
  ExecutionResult RefreshLease(bool is_read_only_lease_refresh) noexcept {
    on_before_lease_acquire_();
    return SuccessExecutionResult();
  }

  std::optional<LeaseInfo> GetCurrentLeaseOwnerInfo() const noexcept {
    return current_lease_owner_info_;
  };

  TimeDuration GetConfiguredLeaseDurationInMilliseconds() const noexcept {
    return lease_duration_in_milliseconds_;
  }

  void SetLeaseDurationInMilliseconds(TimeDuration lease_duration) noexcept {
    lease_duration_in_milliseconds_ = lease_duration;
  }

  bool IsLeaseExpired() noexcept { return is_lease_expired_; }

  bool ShouldRefreshLease() const noexcept { return should_try_refresh_lease_; }

  void SetLeaseOwnerInfo(LeaseInfo lease_owner_info) {
    lease_owner_info_ = lease_owner_info;
  }

  bool IsCurrentLeaseOwner() const noexcept {
    return should_allow_lease_acquire;
  }

  void AllowLeaseAcquire() {
    should_allow_lease_acquire = true;
    current_lease_owner_info_ = lease_owner_info_;
  }

  void DisallowLeaseAcquire() {
    should_allow_lease_acquire = false;
    current_lease_owner_info_.reset();
  }

  void SetOnBeforeAcquireLease(std::function<void()> on_before_lease_acquire) {
    on_before_lease_acquire_ = on_before_lease_acquire;
  }

 private:
  LeaseInfo lease_owner_info_;
  std::optional<LeaseInfo> current_lease_owner_info_;
  std::atomic<bool> should_allow_lease_acquire = {false};
  std::atomic<bool> should_try_refresh_lease_ = {true};
  std::atomic<bool> is_lease_expired_ = {false};
  TimeDuration lease_duration_in_milliseconds_ = 100;
  std::function<void()> on_before_lease_acquire_ = []() {};
};
}  // namespace google::scp::core::lease_manager::mock
