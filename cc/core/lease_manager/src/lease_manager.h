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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/lease_manager_interface.h"

namespace google::scp::core {
/*! @copydoc LeaseManagerInterface
 */
class LeaseManager : public LeaseManagerInterface {
 public:
  /**
   * @brief Construct a new Lease Manager object
   *
   * @param lease_enforcer_frequency_in_milliseconds This is the periodicity of
   * the lease enforcer thread. Lease duration must be higher than this value.
   * @param lease_obtainer_maximum_runningtime_in_milliseconds This is the
   * maximum running time of obtaining lease on LeasableLock + invocation of
   * LeaseTransitionFunction callback. If obtaining lease or invoking callback
   * takes longer than this time duration, the process is terminated. This
   * ensures that the leases are obtained in timely fashion and lease obtainer
   * thread is not stuck.
   */
  LeaseManager(
      TimeDuration lease_enforcer_frequency_in_milliseconds = 500,
      TimeDuration lease_obtainer_maximum_runningtime_in_milliseconds = 3000);

  ExecutionResult ManageLeaseOnLock(
      std::shared_ptr<LeasableLockInterface> leasable_lock,
      LeaseTransitionCallback&& lease_transition_callback) noexcept override;

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

 protected:
  void LeaseObtainerThreadFunction();
  void LeaseEnforcerThreadFunction();

  void NotifyLeaseObtainer(bool obtain_lease);
  void NotifyLeaseEnforcerToStopRunning();

  bool IsLeaseRefreshTakingLong();

  /**
   * NOTE: About threads in this class.
   *
   * There are two threads, lease expiration enforcer and lease obtainer
   *
   * lease expiration enforcer thread:
   * This thread runs every 500ms or 1s (configurable) or so to check if a
   * lease has been expired or needs to be acquired, and instructs the lease
   * obtainer thread to do the work of obtaining a lease or renewing lease.
   *
   * lease obtainer thread:
   * The function of this thread is to call LeasableLock::RefreshLease API to
   * refresh lease i.e. obtain a new lease, and invoke a callback of
   * LeaseTransitionCallback to let the user of lease manager know that
   * something has changed on the lease.
   *
   * If lease obtainer thread takes long to execute the RefreshLease and
   * Callback, lease expiration enforcer terminates the process to recover from
   * getting stuck.
   */

  /**
   * @brief leasable_lock_ is the lock supplied via the ManageLeaseOnLock method
   */
  std::shared_ptr<LeasableLockInterface> leasable_lock_;
  /**
   * @brief lease_transition_callback_ is the callback supplied via the
   * ManageLeaseOnLock method
   */
  LeaseTransitionCallback lease_transition_callback_;

  /**
   * @brief lease_enforcer_thread_ this thread runs the
   * LeaseEnforcerThreadFunction()
   */
  std::unique_ptr<std::thread> lease_enforcer_thread_;
  /**
   * @brief lease_enforcer_thread_started_ represents a T/F whether
   * lease_enforcer_thread_ has started/stopped
   */
  std::atomic<bool> lease_enforcer_thread_started_;
  /**
   * @brief lease_enforcer_thread_mutex_ is a mutex to guard notification on
   * component_running_ via component_running_condvar_
   */
  std::mutex lease_enforcer_thread_mutex_;

  /**
   * @brief lease_obtainer_thread_ this thread runs the
   * LeaseObtainerThreadFunction()
   */
  std::unique_ptr<std::thread> lease_obtainer_thread_;
  /**
   * @brief lease_obtainer_thread_started_ represents a T/F whether the
   * lease_obtainer_thread_ has started/stopped
   */
  std::atomic<bool> lease_obtainer_thread_started_;
  /**
   * @brief lease_obtainer_thread_mutex_ is a mutex to guard notification on
   * obtain_lease_ via obtain_lease_condvar_
   */
  std::mutex lease_obtainer_thread_mutex_;

  /**
   * @brief indicates T/F whether the lease_obtainer_thread_ should obtain a
   * lease upon recieving notification on obtain_lease_condvar_
   */
  std::atomic<bool> obtain_lease_;
  /**
   * @brief condition variable to signal the lease_obtainer_thread_
   */
  std::condition_variable obtain_lease_condvar_;

  /**
   * @brief indicates T/F whether the lease_enforcer_thread_ should stop
   * running upon recieving notification on component_running_condvar_
   */
  std::atomic<bool> component_running_;
  /**
   * @brief condition variable to signal the lease_enforcer_thread_
   */
  std::condition_variable component_running_condvar_;

  /**
   * @brief Captures the start timestamp of lease_obtainer_thread_'s attempt to
   * obtain a lease. This will be reset to a default (0) value upon finishing
   * the attempt. lease_enforcer_thread_ checks this timestamp to see timely
   * finishing of the lease_obtainer_thread_'s attempt
   */
  std::atomic<std::chrono::nanoseconds>
      ongoing_lease_acquisition_start_timestamp_;

  /**
   * @brief Lambda for terminating process
   */
  std::function<void()> terminate_process_function_;

  /**
   * @brief Frequency at which the lease_enforcer_thread_ should check to manage
   * lease on the leasable_lock_
   */
  const std::chrono::milliseconds lease_enforcer_frequency_in_milliseconds_;

  /**
   * @brief Max running time duration threshold after which
   * lease_enforcer_thread_ deems the lease_obtainer_thread_ as stuck and
   * invokes terminate_process_function_
   */
  const std::chrono::milliseconds
      lease_obtainer_maximum_runningtime_in_milliseconds_;
};

};  // namespace google::scp::core
