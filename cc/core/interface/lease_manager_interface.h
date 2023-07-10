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

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {

/**
 * @brief Represents a state transition of lock (implemented with
 * LeasableLockInterface) when a lease acquire attempt is performed by a lease
 * acquirer. Initially, when the system boots up, the lease is not held (lease
 * could actually be held on the lock if the lock is present on a remote server
 * but for our purpose we do not need to know about that), so first lease
 * acquisition would be represented by the state 'kAcquired'. Subsequent lease
 * acquisitions will be represented by the state 'kRenewed'. If the lease is
 * lost for any reason, the transition of it is represented by 'kLost'.
 * Subsequently if lease acquire attempt could not re-acquire lock will be
 * reperesented by 'kNotkAcquired'. If the lease can be re-acquired later, the
 * transition will be represented by 'kAcquired'.
 */
enum class LeaseTransitionType {
  kNotAcquired = 1,
  kAcquired = 2,
  kLost = 3,
  kRenewed = 4
};

/**
 * @brief Info of Lease Acquirer on LeasableLockInterface. This is specific to
 * PBS for now.
 * TODO: Make this be a base and do a derived for PBS specific stuff.
 */
struct LeaseInfo {
  /**
   * @brief unique identifer of lease acquirer
   */
  std::string lease_acquirer_id;
  /**
   * @brief Endpoint address of the PBS service of the lease acquirer
   *
   */
  std::string service_endpoint_address;
};

/**
 * @brief For each transition represented by LeaseTransitionType, a
 * user-supplied callback of this will be invoked when the transition happens.
 * LeaseInfo represents the current lease holder (if any).
 */
typedef std::function<void(LeaseTransitionType, std::optional<LeaseInfo>)>
    LeaseTransitionCallback;

/**
 * @brief Interface to implement lease semantics on top of an existing or new
 * lock type.
 */
class LeasableLockInterface {
 public:
  virtual ~LeasableLockInterface() = default;

  /**
   * @brief Check if lease acquisition (if a non-lease owner) or lease
   * renewal (if a lease owner) is required on the lock.
   *
   * @return true
   * @return false
   */
  virtual bool ShouldRefreshLease() const noexcept = 0;

  /**
   * @brief Acquires or Renews lease on the lock. Existing lease owner would
   * renew, otherwise lease would be freshly acquired.
   * NOTE: lease duration is left to implementation of this interface.
   *
   * @return ExecutionResult If lease acquisition attempt went through, returns
   * with a success, else failure.
   */
  virtual ExecutionResult RefreshLease() noexcept = 0;

  /**
   * @brief Get configured lease duration.
   * NOTE: lease duration is left to implementation of this interface.
   *
   * @return TimeDuration ticks in milliseconds
   */
  virtual TimeDuration GetConfiguredLeaseDurationInMilliseconds()
      const noexcept = 0;

  /**
   * @brief Get current Lease Owner's information
   *
   * @return LeaseInfo or none if information cannot be obtained or is
   * stale in the implementations cache.
   */
  virtual std::optional<LeaseInfo> GetCurrentLeaseOwnerInfo()
      const noexcept = 0;

  /**
   * @brief Is caller a lease owner on the lock at this moment.
   *
   * @return True if a lease owner, else False.
   */
  virtual bool IsCurrentLeaseOwner() const noexcept = 0;
};

/**
 * @brief LeaseManagerInterface provides interface for lease acquisition and
 * maintenance.
 */
class LeaseManagerInterface : public ServiceInterface {
 public:
  /**
   * @brief ManageLeaseOnLock
   *
   * Register a LeasableLock to acquire and maintain lease on it. If a
   * LeaseTransitionType happens, corresponding user supplied callback will be
   * invoked.
   *
   * @param leasable_lock lock on which lease needs to be acquired and
   * renewed/maintained.
   * @param lease_transition_callback lambda that gets invoked when a
   * LeaseTransitionType happens.
   * @return ExecutionResult Success if the lock can be managed, Failure if it
   * cannot be managed.
   */
  virtual ExecutionResult ManageLeaseOnLock(
      std::shared_ptr<LeasableLockInterface> leasable_lock,
      LeaseTransitionCallback&& lease_transition_callback) noexcept = 0;
};
};  // namespace google::scp::core
