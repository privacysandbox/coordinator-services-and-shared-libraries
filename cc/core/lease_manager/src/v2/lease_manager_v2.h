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
#include <thread>
#include <unordered_map>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/lease_manager_interface.h"

namespace google::scp::core {

static LeaseAcquisitionPreference kDefaultLeaseAcquisitionPreference = {
    0 /* no locks should be held as a default */,
    {} /* no preference on which locks to hold lease on */};

/**
 * @brief LeaseManagerV2
 *
 * LeaseManagerV2 manages leases simultaenously on multiple LeasableLocks.
 * This is different from LeaseManager where only a single LeasableLock can be
 * managed.
 *
 * ManageLeaseOnLock() API can be invoked multiple times to specify the set of
 * LeasableLocks to be managed. Lease acquisition preferences can be tuned at
 * any time to adjust the number of leases to be held at any time.
 *
 **! @copydoc LeaseManagerV2Interface
 **! @copydoc LeaseAcquisitionPreferenceInterface
 **! @copydoc LeaseReleaseNotificationInterface
 */
class LeaseManagerV2 : public LeaseManagerV2Interface,
                       public LeaseAcquisitionPreferenceInterface,
                       public LeaseReleaseNotificationInterface,
                       public LeaseStatisticsInterface {
 public:
  LeaseManagerV2(
      std::shared_ptr<LeaseRefresherFactoryInterface> lease_refresher_factory,
      LeaseAcquisitionPreference lease_acquisition_preference =
          kDefaultLeaseAcquisitionPreference);

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  /**
   * @brief Add a leasable lock to manage.
   *
   * NOTE: This is not thread-safe and should not be invoked concurrently.
   *
   * @param leasable_lock_id
   * @param leasable_lock
   * @param lease_event_sink
   * @return ExecutionResult
   */
  ExecutionResult ManageLeaseOnLock(
      const LeasableLockId& leasable_lock_id,
      const std::shared_ptr<LeasableLockInterface>& leasable_lock,
      const std::shared_ptr<LeaseEventSinkInterface>& lease_event_sink) noexcept
      override;

  /**
   * @brief Set the Lease Acquisition Preference. Leases will be acquired
   * according to the preference specified.
   *
   * NOTE: This is not thread-safe and should not be invoked concurrently.
   *
   * @param preference
   * @return ExecutionResult
   */
  ExecutionResult SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference preference) noexcept override;

  /**
   * @brief Notification from a subscriber that the lease is safe to be
   * released.
   *
   * @param leasable_lock_id
   */
  void SafeToReleaseLease(
      const LeasableLockId& leasable_lock_id) noexcept override;

  /**
   * @brief Perform a round of lease acquisition management. This is invoked by
   * internal thread and can also be invoked by an external actor when needed.
   *
   */
  void PerformLeaseAcquisitionPreferenceManagement();

  size_t GetCurrentlyLeasedLocksCount() noexcept override;

 protected:
  void LeaseAcquisitionPreferenceManagerThreadFunction();

  void TryHoldMoreLeases(size_t leases_to_hold_count);

  void TryReleaseSomeLeases(size_t number_of_leases_to_release);

  void RevertTheIntentionToHoldLeaseForLocksWhichCannotBeLeased();

  /**
   * @brief Wraps lease refresh related things.
   */
  struct LeaseRefresherWrapper {
    /// @brief Lease refresher handle
    std::shared_ptr<LeaseRefresherInterface> lease_refresher_handle;
    /// @bried Leasable Lock
    std::shared_ptr<LeasableLockInterface> leasable_lock;
  };

  /// @brief Indicates if the component is running.
  std::atomic<bool> is_running_;
  /// @brief Factory interface for constructing Lease Refresher objects.
  std::shared_ptr<LeaseRefresherFactoryInterface> lease_refresher_factory_;
  /// @brief The lease refreshers. This is read-only once initialized so no need
  /// of lock.
  std::unordered_map<LeasableLockId, LeaseRefresherWrapper, common::UuidHash>
      lease_refreshers_;
  /// @brief Lease refresh liveness enforcer
  std::unique_ptr<LeaseRefreshLivenessEnforcerInterface>
      lease_refresh_liveness_enforcer_;
  /// @brief Lease acqusition preference set by the client.
  LeaseAcquisitionPreference lease_acquisition_preference_;
  /// @brief Mutex to protect the lease preference setting
  std::mutex lease_preference_mutex_;
  /// @brief Mutex to protect the lease preference management invocations
  std::mutex lease_preference_manager_mutex_;
  /// @brief Lease preference run thread
  std::unique_ptr<std::thread> lease_preference_manager_thread_;
  /// @brief Object activity ID
  core::common::Uuid activity_id_;
};
}  // namespace google::scp::core
