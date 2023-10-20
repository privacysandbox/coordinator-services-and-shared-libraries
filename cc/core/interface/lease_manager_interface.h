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
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/service_interface.h"
#include "core/interface/type_def.h"

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
  /**
   * @brief Lease Not Acquired: This transition happens when a lease cannot be
   * acquired. This happens if the lock is not available to hold lease on due to
   * some systemic issues, or an other owner already owns a lease on the lock.
   */
  kNotAcquired = 1,
  /**
   * @brief Lease Acquired: This transition happens when a lease is acquired.
   * This happens when the lock's lease is obtained by the instance.
   */
  kAcquired = 2,
  /**
   * @brief Lease Lost: This transition happens when a lease is lost because the
   * lease duration has elapsed. This could be due to some system issue or
   * unable to renew lease because some other instance is now holding lease.
   */
  kLost = 3,
  /**
   * @brief Lease Renewed: This transition happens when a lease is extended
   * while holding the lease.
   */
  kRenewed = 4,
  /**
   * The following lease transitions are generated EXCLUSIVELY by
   * LeaseRefresherInterface used by LeaseManagerV2.
   */
  /**
   * @brief Lease Renewed With Intention To Release: This transition happens
   * when a lease (an acquired lease) is going to be released gracefully by the
   * Lease Refresher. This is a hint to the subscriber to start doing last rites
   * before the lease is released. Lease will only be released after subscriber
   * invokes SafeToReleaseLease() API via LeaseReleaseNotificationInterface on
   * Lease Manager after the last rites are done, until then the lease will be
   * continously renewed. If lease cannot be renewed, then an ungraceful kLost
   * transition is done.
   */
  kRenewedWithIntentionToRelease = 5,
  /**
   * @brief Lease Released: Once the lease is safe to be released via invocation
   * of the SafeToReleaseLease() API, the lease is released and this transition
   * is invoked as a result to notify that the graceful release of lease is
   * completed.
   */
  kReleased = 6
};

/**
 * @brief Unique identification for a LeasableLock. For a PBS partition lock,
 * this usually is PartitionID.
 */
typedef common::Uuid LeasableLockId;

/**
 * @brief Info of currently acquired lease or acquirer of a lease on
 * LeasableLockInterface. This is specific to PBS for now.
 * TODO: Make this be a base and do a derived for PBS specific stuff.
 */
struct LeaseInfo {
  /**
   * @brief unique identifer of lease acquirer
   */
  std::string lease_acquirer_id;
  /**
   * @brief Endpoint address of the PBS service of the lease acquirer.
   */
  std::string service_endpoint_address;

  /**
   * @brief Equal operator.
   */
  bool operator==(const LeaseInfo& other) const {
    return (lease_acquirer_id == other.lease_acquirer_id) &&
           (service_endpoint_address == other.service_endpoint_address);
  }
};

/**
 * @brief Structure to hold lease acquisition preference. Lease acquisition
 * preference can be specified by the callers to change how Lease manager
 * acquires leases on the locks.
 */
struct LeaseAcquisitionPreference {
  /// Maximum number of leases to hold among the leasable locks.
  size_t maximum_number_of_leases_to_hold;
  /// The set of leasable locks preferrably on which the
  /// 'maximum_number_of_leases_to_hold' number of leases needed to be
  /// acquired. If this is empty, the 'maximum_number_of_leases_to_hold'
  /// leases can be acquired on any locks.
  std::vector<LeasableLockId> preferred_locks_to_acquire_leases_on;

  // Equal comparision operator.
  bool operator==(const LeaseAcquisitionPreference& other) const {
    return maximum_number_of_leases_to_hold ==
               other.maximum_number_of_leases_to_hold &&
           preferred_locks_to_acquire_leases_on ==
               other.preferred_locks_to_acquire_leases_on;
  }
};

/**
 * @brief For each transition represented by LeaseTransitionType, a
 * user-supplied callback of this will be invoked when the transition happens.
 * LeaseInfo represents the current lease holder (if any).
 *
 * NOTE: current lease holder is optional, as at times when the lease is lost
 * due to systemic issues, the current lease owner cannot be determined. Also,
 * this is not populated (not necessary) for the
 * LeaseTransitionType::kAcquired, LeaseTransitionType::kLost,
 * LeaseTransitionType::kRenewed types since the owner is inferred to be the
 * current instance with these events.
 *
 */
typedef std::function<void(LeaseTransitionType, std::optional<LeaseInfo>)>
    LeaseTransitionCallback;

/**
 * @brief Interface to implement sink of lease transition events. The
 * methods of this interface are invoked by the Lease Refresher
 * (LeaseRefresherInterface) to communicate the lease transition events to a
 * subscriber of Lease Manager. See LeaseManagerInterface for details on how
 * the subscription is established.
 */
class LeaseEventSinkInterface {
 public:
  virtual ~LeaseEventSinkInterface() = default;
  /**
   * @brief OnLeaseTransition.
   *
   * This method is invoked upon a lease transition on the leasable lock.
   *
   * -------------------------------------------------------------------
   *                          Scenarios
   * -------------------------------------------------------------------
   * a) Lease is acquired and is lost due to network disconnectivity.
   * (UNGRACEFUL lease transition)
   * -------------------------------------------------------------------
   * The following are the series of events which occur in the order specified.
   * Events are mutually exclusive i.e. only after one event an other one is
   * issued.
   * 1. LeaseTransitionType::kNotAcquired (1..N times)
   * 2. LeaseTransitionType::kAcquired    (1 time)      [LEASE ACQUIRED]
   * 3. LeaseTransitionType::kRenewed     (1..N times)
   * 4. LeaseTransitionType::kLost        (1 time)      [LEASE GONE]
   * 5. LeaseTransitionType::kNotAcquired (1..N times)
   * -------------------------------------------------------------------
   * b) Lease is acquired and is released due to load balancing event
   * (GRACEFUL lease transition)
   * -------------------------------------------------------------------
   * The following are the series of events which occur in the order specified.
   * Events are mutually exclusive i.e. only after one event an other one
   * is issued.
   * 1. LeaseTransitionType::kNotAcquired (1..N times)
   * 2. LeaseTransitionType::kAcquired    (1 time)      [LEASE ACQUIRED]
   * 3. LeaseTransitionType::kRenewed     (1..N times)
   * 4. LeaseTransitionType::kRenewedWithIntentionToRelease
   *                                      (1..N times)
   * 5. LeaseTransitionType::kReleased    (1 time)      [LEASE GONE]
   * 6. LeaseTransitionType::kNotAcquired (1..N times)
   * -------------------------------------------------------------------
   * c) Lease is never acquired (it is acquired by another instance).
   * -------------------------------------------------------------------
   * The following are the series of events which occur in the order specified.
   * Events are mutually exclusive i.e. only after one event an other one is
   * issued.
   * 1. LeaseTransitionType::kNotAcquired (1..N times)
   * -------------------------------------------------------------------
   *
   * NOTE: Running time of this method should be quick to ensure liveness of
   * LeaseRefresher.
   */
  virtual void OnLeaseTransition(const LeasableLockId&, LeaseTransitionType,
                                 std::optional<LeaseInfo>) noexcept = 0;
};

/**
 * @brief Interface to implement lease semantics on top of an existing or new
 * lock type.
 */
class LeasableLockInterface {
 public:
  virtual ~LeasableLockInterface() = default;

  /**
   * @brief Check if lease acquisition (if a not a lease owner) or lease
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
   * @param is_read_only_lease_refresh This indicates that the lease
   * refresher should refresh the lease (i.e. read only) without acquiring it.
   *
   * @return ExecutionResult If lease acquisition attempt went through, returns
   * with a success, else failure.
   */
  virtual ExecutionResult RefreshLease(
      bool is_read_only_lease_refresh) noexcept = 0;

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
   * @return LeaseInfo of the current lease owner or nullopt if
   * lease owner does not exist.
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

class LeaseStatisticsInterface {
 public:
  /**
   * @brief Get the current count of the locks on which a lease is held by this
   * or any other lease manager.
   *
   * @return size_t
   */
  virtual size_t GetCurrentlyLeasedLocksCount() noexcept = 0;
};

/**
 * @brief LeaseManagerV2Interface provides interface for lease acquisition and
 * lease maintenance on multiple LeasableLocks.
 *
 * This is an improved version of the LeaseManagerInterface. For code
 * backwards compatibility reasons and other non-technical reasons, the
 * LeaseManagerInterface is not touched.
 *
 */
class LeaseManagerV2Interface : public ServiceInterface {
 public:
  virtual ~LeaseManagerV2Interface() = default;
  /**
   * @brief ManageLeaseOnLock
   *
   * Register a LeasableLock to acquire and maintain lease on it. If lease
   * transition event occurs on a lock then user supplied callback will be
   * invoked with details of the transition.
   *
   * NOTE: This method is invoked before Init() of LeaseManagerInterface.
   * New leasable locks cannot be added dynamically, but leases on existing
   * leasable locks can be dynamically adjusted with help of
   * LeaseAcquisitionPreference interface if implemented.
   *
   * @param leasable_lock_id id of the leasable lock.
   * @param leasable_lock lock on which lease needs to be acquired and
   * renewed/maintained.
   * @param lease_event_sink lease event sink where lease events will be
   * delivered.
   *
   * @return ExecutionResult Success if the lock can be accepted, Failure if it
   * cannot be accepted.
   */
  virtual ExecutionResult ManageLeaseOnLock(
      const LeasableLockId& leasable_lock_id,
      const std::shared_ptr<LeasableLockInterface>& leasable_lock,
      const std::shared_ptr<LeaseEventSinkInterface>&
          lease_event_sink) noexcept = 0;
};

/**
 * @brief Interface to specify how many leases and which leases to acquire to
 * the LeaseManager
 */
class LeaseAcquisitionPreferenceInterface {
 public:
  /**
   * @brief This allows a caller to adjust the maximum number of leases to be
   * held on the LeasableLocks specified by ManageLeaseOnLock API. Optionally,
   * a set of LeasableLockIds can be specified to instruct the Lease Manager to
   * acquire Leases *preferably* on the specified Locks. Acquiring Leases on
   * the specified Locks is not guaranteed as the current owner (maybe another
   * instance) might need to relinquish its ongoing lease on the specified Lock.
   * Having this API can be useful if caller wants to dynamically adjust the
   * number of Leases to be held, such as number of partitions to be owned by
   * the instance based on current load conditions in the system. And also to
   * pin a specific Lock to a specific instance of Lease Manager by specifying
   * it in the preferred_locks_to_acquire_leases_on on that Lease Manager
   * (example below).
   *
   * NOTE: For this API to take effect, the same API needs to be invoked on the
   * other instances of Lease Manager to provide exclusive values. For example,
   * if there are 30 partitions in the system, and 3 instances of Lease Manager.
   * All Lease Managers manage leases on 30 partitions, but if Lease Manager 1
   * needs to acquire leases on 29 of the 30 locks, and Lease Manager 2 the
   * remaining lock, then the following invocations to be done.
   *
   * 1) On Lease Manager 1 SetLeaseAcquisitionPreference({29, {}});
   * 2) On Lease Manager 2 SetLeaseAcquisitionPreference({1, {}});
   * 3) On Lease Manager 3 SetLeaseAcquisitionPreference({0, {}});
   *
   * @param lease_acquisition_preference
   * @return ExecutionResult
   */
  virtual ExecutionResult SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference lease_acquisition_preference) noexcept = 0;
};

/**
 * LeaseRefreshMode
 *
 * @brief Desired type of lease refresh to be done by the
 * LeaseRefresherInterface. Subscriber of the Lease is notified via
 * LeaseTransitionCallback of any transition that happens due to a setting of
 * lease refresh mode on a LeaseRefresher.
 *
 * Transitions
 * ===========
 *
 * All of the transitions are directed by external agent such as the Lease
 * Manager using the TrySetLeaseRefreshMode() API. No internally driven
 * transitions are done within the Lease Refresher.
 *
 * -> By default a Lease Refresher starts with
 * 'RefreshWithNoIntentionToHoldLease' mode. Lease will not be acquired/held.
 * -> 'RefreshWithNoIntentionToHoldLease' can be moved only to
 * 'RefreshWithIntentionToHoldLease'. Lease acquisition will be attempted.
 * -> From 'RefreshWithIntentionToHoldLease', the lease refresher can move to
 * 'RefreshWithNoIntentionToHoldLease' only if there is no lease acquired/held
 * currently.
 * -> From 'RefreshWithIntentionToHoldLease', the lease refresher can go to
 * 'RefreshWithIntentionToReleaseTheHeldLease' any time if acquired/held lease
 * needs to be given up gracefully.
 * -> From 'RefreshWithIntentionToReleaseTheHeldLease', the lease refresher can
 * go to 'RefreshWithNoIntentionToHoldLease'. This makes the acquired lease to
 * be released.
 */
enum class LeaseRefreshMode {
  Unknown,
  /**
   * @brief Lease is acquired if possible as part of refresh.
   */
  RefreshWithIntentionToHoldLease,
  /**
   * @brief Lease is re-acquired (renewed) as part of refresh. If lease is lost
   * for any reason, it will not be reacquired, and for the subsequent refresh
   * attempts this mode boils down to 'RefreshWithNoIntentionToHoldLease' mode.
   * The subscriber will be notified at each lease transition (via
   * LeaseTransitionCallback) that the lease is going to be released soon. This
   * allows for implementing graceful handover of lease to another instance's
   * lease manager without causing disruption to the subscriber's ongoing work
   * related to the lease.
   */
  RefreshWithIntentionToReleaseTheHeldLease,
  /**
   * @brief Lease refresh is done without lease acquisition. Only the current
   * lease information is read from the lock and updated into the LeasableLock.
   * This is useful to passively read the current ownership of lease and use
   * that information in routing and other processes, and also to eventually own
   * the lease if no current owner exists.
   */
  RefreshWithNoIntentionToHoldLease,
};

/**
 * LeaseRefresherInterface
 *
 * @brief LeaseRefresher is a sub-component of LeaseManager, and its
 * responsibility is to attempt refreshing (acquire or release)
 * Lease on a LeasableLockInterface. Lease Manager consists of one or more
 * LeaseRefreshers and each LeaseRefresher operates on a
 * single LeasableLockInterface.
 *
 * An implementation of the LeaseRefresherInterface would have functionality to
 * refresh lease periodically according to the LeaseDuration specified on the
 * underlying LeasableLock. There are 3 modes of lease refreshes possible as
 * specified in LeaseRefreshMode enum.
 */
class LeaseRefresherInterface : public ServiceInterface {
 public:
  virtual ~LeaseRefresherInterface() = default;

  /**
   * @brief Get the current lease refresh mode.
   */
  virtual LeaseRefreshMode GetLeaseRefreshMode() const noexcept = 0;

  /**
   * @brief Set refresh mode for the lease. The implemention should guarantee
   * the mutual exclusion of setting the mode w.r.t. an ongoing lease refresh.
   * In other words, if there is an ongoing lease refresh, the mode cannot be
   * changed and the API needs to wait until the lease refresh completes.
   *
   * NOTE: Please refer to LeaseRefreshMode header comment for rules on mode
   * transitions that an implementation of this interface should honor.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult SetLeaseRefreshMode(
      LeaseRefreshMode lease_refresh_mode) noexcept = 0;

  /**
   * @brief Perform refresh on the lease.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult PerformLeaseRefresh() noexcept = 0;
};

/**
 * @brief Interface to implement a liveness checker for Lease Refresher. This
 * ensures freshness of Lease Refresh and allows Lease Manager to enforce
 * liveness by a corrective action such as terminating process, etc.
 */
class LeaseRefreshLivenessCheckInterface {
 public:
  virtual ~LeaseRefreshLivenessCheckInterface() = default;
  /**
   * @brief Returns the timestamp of the last completed
   * refresh attempt. This allows an observer of LeaseRefresher to check
   * liveness.
   *
   * NOTE: The returned timestamp is a steady timestamp, i.e. it cannot be
   * used as a wall-clock timestamp.
   */
  virtual std::chrono::nanoseconds GetLastLeaseRefreshTimestamp()
      const noexcept = 0;
};

/**
 * @brief Interface to construct the LeaseRefresher objects
 */
class LeaseRefresherFactoryInterface {
 public:
  virtual ~LeaseRefresherFactoryInterface() = default;
  /**
   * @brief Construct an object of LeaseRefresherInterface.
   *
   * @param leasable_lock
   * @param lease_transition_callback
   * @return std::shared_ptr<LeaseRefresherInterface>
   */
  virtual std::shared_ptr<LeaseRefresherInterface> Construct(
      const LeasableLockId& leasable_lock_id,
      const std::shared_ptr<LeasableLockInterface>& leasable_lock,
      const std::shared_ptr<LeaseEventSinkInterface>&
          lease_event_sink) noexcept = 0;
};

/**
 * @brief Interface for subscribers of LeaseManager to notify LeaseManager to
 * release lease.
 */
class LeaseReleaseNotificationInterface {
 public:
  /**
   * @brief Notification that lease on the leasable_lock_id can be released if
   * lease is currently held. This is for a external subscriber to gracefully do
   * any last rites, such as Partition unload in PBS's partition manager, and
   * notify the lease manager that the lease can be safely released/transferred
   * to another instance's Lease Manager.
   *
   * @param leasable_lock_id
   */
  virtual void SafeToReleaseLease(
      const LeasableLockId& leasable_lock_id) noexcept = 0;
};

/**
 * @brief Interface to implement a lease liveness enforcer that can monitor all
 * of the lease refreshers and ensures that they are making progress in
 * refreshing lease.
 *
 */
class LeaseRefreshLivenessEnforcerInterface : public ServiceInterface {
 public:
  /**
   * @brief Enforce liveness check on all the refreshers.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult PerformLeaseLivenessEnforcement() noexcept = 0;
};

};  // namespace google::scp::core
