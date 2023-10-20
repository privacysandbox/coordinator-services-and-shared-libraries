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

namespace google::scp::pbs::leasable_lock::mock {
class MockLeasableLock : public core::LeasableLockInterface {
 public:
  explicit MockLeasableLock(core::TimeDuration lease_duration)
      : lease_duration_(lease_duration) {}

  bool ShouldRefreshLease() const noexcept override {
    return should_refresh_lease_;
  }

  core::ExecutionResult RefreshLease(
      bool is_read_only_lease_refresh = false) noexcept override {
    return core::SuccessExecutionResult();
  }

  core::TimeDuration GetConfiguredLeaseDurationInMilliseconds()
      const noexcept override {
    return lease_duration_;
  }

  std::optional<core::LeaseInfo> GetCurrentLeaseOwnerInfo()
      const noexcept override {
    std::unique_lock lock(mutex_);
    return current_lease_owner_info_;
  }

  void SetCurrentLeaseOwnerInfo(core::LeaseInfo lease_info) noexcept {
    std::unique_lock lock(mutex_);
    current_lease_owner_info_ = lease_info;
  }

  bool IsCurrentLeaseOwner() const noexcept override { return is_owner_; }

  mutable std::mutex mutex_;
  core::LeaseInfo current_lease_owner_info_ = {};
  std::atomic<bool> is_owner_ = false;
  core::TimeDuration lease_duration_;
  std::atomic<bool> should_refresh_lease_ = true;
};
}  // namespace google::scp::pbs::leasable_lock::mock
