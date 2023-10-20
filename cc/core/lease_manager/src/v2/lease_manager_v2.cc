// Copyright 2022 Google LLC
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

#include "lease_manager_v2.h"

#include <memory>
#include <optional>
#include <vector>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/lease_manager_interface.h"
#include "core/lease_manager/src/v2/lease_refresh_liveness_enforcer.h"

#include "error_codes.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using std::atomic;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::move;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;

static constexpr milliseconds
    kLeaseAcquisitionPreferenceInvocationIntervalInMilliseconds =
        milliseconds(1000);

static constexpr char kLeaseManagerV2[] = "LeaseManagerV2";

namespace google::scp::core {

LeaseManagerV2::LeaseManagerV2(
    shared_ptr<LeaseRefresherFactoryInterface> lease_refresher_factory,
    LeaseAcquisitionPreference lease_acquisition_preference)
    : is_running_(false),
      lease_refresher_factory_(lease_refresher_factory),
      lease_acquisition_preference_(lease_acquisition_preference),
      activity_id_(Uuid::GenerateUuid()) {}

ExecutionResult LeaseManagerV2::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult LeaseManagerV2::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(errors::SC_LEASE_MANAGER_ALREADY_RUNNING);
  }
  is_running_ = true;

  // Initialize the lease refresher liveness enforcer with the set of lease
  // refreshers that were added. This cannot be done in the Init() because
  // refreshers aren't available at the Init() time.
  if (!lease_refreshers_.empty()) {
    // Initialize Lease Liveness Enforcer
    vector<shared_ptr<LeaseRefreshLivenessCheckInterface>>
        lease_refresher_liveness_handles;
    lease_refresher_liveness_handles.reserve(lease_refreshers_.size());
    for (auto& [_, refresher_wrapper] : lease_refreshers_) {
      RETURN_IF_FAILURE(refresher_wrapper.lease_refresher_handle->Init());
      lease_refresher_liveness_handles.push_back(
          dynamic_pointer_cast<LeaseRefreshLivenessCheckInterface>(
              refresher_wrapper.lease_refresher_handle));
    }

    // An arbitrary lease refresher is chosen here as all of them have the
    // same lease duration. We do not support different lease durations for
    // refreshers yet, although not enforced.
    auto lease_duration_in_milliseconds = milliseconds(
        lease_refreshers_.begin()
            ->second.leasable_lock->GetConfiguredLeaseDurationInMilliseconds());
    lease_refresh_liveness_enforcer_ =
        make_unique<LeaseRefreshLivenessEnforcer>(
            lease_refresher_liveness_handles, lease_duration_in_milliseconds);
    RETURN_IF_FAILURE(lease_refresh_liveness_enforcer_->Init());

    SCP_INFO(kLeaseManagerV2, activity_id_,
             "Initialized Liveness Enforcer with '%llu' refreshers",
             lease_refreshers_.size());
  }

  // Start Lease Refreshers
  for (auto& [_, refresher_wrapper] : lease_refreshers_) {
    RETURN_IF_FAILURE(refresher_wrapper.lease_refresher_handle->Run());
  }

  // Start Lease Refresh Liveness Enforcer.
  if (lease_refresh_liveness_enforcer_) {
    RETURN_IF_FAILURE(lease_refresh_liveness_enforcer_->Run());
  }

  // Start Lease Preference Manager.
  // Initiate start on thread and wait until it starts.
  atomic<bool> is_thread_started(false);
  lease_preference_manager_thread_ =
      make_unique<thread>([this, &is_thread_started]() {
        is_thread_started = true;
        LeaseAcquisitionPreferenceManagerThreadFunction();
      });
  while (!is_thread_started) {
    sleep_for(milliseconds(100));
  }

  return SuccessExecutionResult();
}

ExecutionResult LeaseManagerV2::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_LEASE_MANAGER_NOT_RUNNING);
  }
  is_running_ = false;

  // Stop Lease Preference Manager
  if (lease_preference_manager_thread_->joinable()) {
    lease_preference_manager_thread_->join();
  }

  // Stop Lease Refresh Liveness Enforcer.
  if (lease_refresh_liveness_enforcer_) {
    RETURN_IF_FAILURE(lease_refresh_liveness_enforcer_->Stop());
  }

  // Stop Lease Refreshers
  for (auto& [_, refresher_wrapper] : lease_refreshers_) {
    RETURN_IF_FAILURE(refresher_wrapper.lease_refresher_handle->Stop());
  }
  return SuccessExecutionResult();
}

ExecutionResult LeaseManagerV2::ManageLeaseOnLock(
    const LeasableLockId& leasable_lock_id,
    const shared_ptr<LeasableLockInterface>& leasable_lock,
    const shared_ptr<LeaseEventSinkInterface>& lease_event_sink) noexcept {
  if (is_running_) {
    // Cannot manage a new one while the component is running
    return FailureExecutionResult(errors::SC_LEASE_MANAGER_ALREADY_RUNNING);
  }

  auto lease_refresher = lease_refresher_factory_->Construct(
      leasable_lock_id, leasable_lock, lease_event_sink);
  lease_refreshers_[leasable_lock_id] =
      LeaseRefresherWrapper{lease_refresher, leasable_lock};
  return SuccessExecutionResult();
}

ExecutionResult LeaseManagerV2::SetLeaseAcquisitionPreference(
    LeaseAcquisitionPreference preference) noexcept {
  unique_lock<mutex> lock(lease_preference_mutex_);
  lease_acquisition_preference_ = preference;
  return SuccessExecutionResult();
}

void LeaseManagerV2::SafeToReleaseLease(
    const LeasableLockId& leasable_lock) noexcept {
  if (!is_running_) {
    SCP_ERROR(
        kLeaseManagerV2, activity_id_,
        FailureExecutionResult(
            errors::SC_LEASE_MANAGER_RELEASE_NOTIFICATION_CANNOT_BE_PROCESSED),
        "Lease Manager not running. Cannot process SafeToReleaseLease "
        "notification for the Lock with ID: '%s'. Returning.",
        ToString(leasable_lock).c_str());
    return;
  }

  auto it = lease_refreshers_.find(leasable_lock);
  if (it == lease_refreshers_.end()) {
    SCP_ERROR(
        kLeaseManagerV2, activity_id_,
        FailureExecutionResult(
            errors::SC_LEASE_MANAGER_RELEASE_NOTIFICATION_CANNOT_BE_PROCESSED),
        "Lock with ID: '%s' not found. Returning.",
        ToString(leasable_lock).c_str());
    return;
  }
  auto& [lock_id, refresher_wrapper] = *it;
  // Assumption is that, if the refresher is in the
  // RefreshWithIntentionToReleaseTheHeldLease mode, then the only actor
  // driving the change in mode to RefreshWithNoIntentionToHoldLease is this
  // code path, so there is no need of compare and swap operation here.
  if (refresher_wrapper.lease_refresher_handle->GetLeaseRefreshMode() !=
      LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease) {
    SCP_ERROR(
        kLeaseManagerV2, activity_id_,
        FailureExecutionResult(
            errors::SC_LEASE_MANAGER_RELEASE_NOTIFICATION_CANNOT_BE_PROCESSED),
        "Lock with ID: '%s' is not in the Release mode. Returning.",
        ToString(leasable_lock).c_str());
    return;
  }

  SCP_INFO(kLeaseManagerV2, activity_id_,
           "Setting 'RefreshWithNoIntentionToHoldLease' on the Lock: '%s'",
           ToString(lock_id).c_str());

  // TRANSITION:
  // RefreshWithIntentionToReleaseTheHeldLease ==>>>
  // RefreshWithNoIntentionToHoldLease
  auto execution_result =
      refresher_wrapper.lease_refresher_handle->SetLeaseRefreshMode(
          LeaseRefreshMode::RefreshWithNoIntentionToHoldLease);
  if (!execution_result.Successful()) {
    // This should not happen. Crash to recover from this.
    SCP_EMERGENCY(
        kLeaseManagerV2, activity_id_,
        FailureExecutionResult(
            core::errors::
                SC_LEASE_REFRESHER_INVALID_STATE_WHEN_SETTING_REFRESH_MODE),
        "Setting 'RefreshWithNoIntentionToHoldLease' on the Lock: "
        "'%s' failed");
    std::abort();
  }
}

// Should be invoked under lock on lease_preference_manager_mutex_
void LeaseManagerV2::TryHoldMoreLeases(size_t new_leases_to_hold_count) {
  // Step 1) Check the ones already set to hold lease and effectively
  // changing the leases to hold count.
  size_t intention_set_count = 0;
  size_t intention_fulfilled_count = 0;
  for (auto& [_, refresher_wrapper] : lease_refreshers_) {
    if (refresher_wrapper.lease_refresher_handle->GetLeaseRefreshMode() ==
        LeaseRefreshMode::RefreshWithIntentionToHoldLease) {
      intention_set_count++;
      // Once the intention is set, the lock will eventually leased and then
      // the intention is said to be honored on the lock. If the intention
      // is not honored (because some one already is leasing), then the
      // lock's intention to hold lease will be reverted, see
      // RevertTheIntentionToHoldLeaseForLocksWhichCannotBeLeased()
      // function.
      if (refresher_wrapper.leasable_lock->IsCurrentLeaseOwner()) {
        intention_fulfilled_count++;
      }
    }
  }

  SCP_INFO(kLeaseManagerV2, activity_id_,
           "Trying to lease '%llu' new leases. Intention Set "
           "Count: '%llu', Intention Fulfilled Count: '%llu'",
           new_leases_to_hold_count, intention_set_count,
           intention_fulfilled_count);

  // We first need to wait for existing ones to be fulfilled before starting
  // to acquire new ones.
  auto leasing_in_progress_count =
      (intention_set_count - intention_fulfilled_count);
  if (leasing_in_progress_count >= new_leases_to_hold_count) {
    // already in the process of leasing, nothing to do now.
    return;
  }

  size_t effective_leases_to_hold_count =
      new_leases_to_hold_count - leasing_in_progress_count;
  if (effective_leases_to_hold_count == 0) {
    return;
  }

  // Step 2) Set the new ones to hold.
  for (auto& [lock_id, refresher_wrapper] : lease_refreshers_) {
    if (effective_leases_to_hold_count == 0) {
      break;
    }
    auto no_one_currently_holding_lease =
        !refresher_wrapper.leasable_lock->GetCurrentLeaseOwnerInfo()
             .has_value();
    if (refresher_wrapper.lease_refresher_handle->GetLeaseRefreshMode() ==
            LeaseRefreshMode::RefreshWithNoIntentionToHoldLease &&
        no_one_currently_holding_lease) {
      SCP_INFO(kLeaseManagerV2, activity_id_,
               "Setting 'RefreshWithIntentionToHoldLease' on the Lock: '%s'",
               ToString(lock_id).c_str());

      // TRANSITION:
      // RefreshWithNoIntentionToHoldLease ====>>>
      // RefreshWithIntentionToHoldLease
      auto execution_result =
          refresher_wrapper.lease_refresher_handle->SetLeaseRefreshMode(
              LeaseRefreshMode::RefreshWithIntentionToHoldLease);
      if (!execution_result.Successful()) {
        SCP_ERROR(kLeaseManagerV2, activity_id_, execution_result,
                  "Setting 'RefreshWithIntentionToHoldLease' on the Lock: '%s' "
                  "failed. Will retry in the next round.",
                  ToString(lock_id).c_str());
        continue;
      }

      effective_leases_to_hold_count--;
    }
  }
}

// Should be invoked under lock on lease_preference_manager_mutex_
void LeaseManagerV2::TryReleaseSomeLeases(size_t leases_to_release_count) {
  // Step 1) Check the ones already set to release lease and
  // changing the leases to release count accordingly.
  size_t intention_set_count = 0;
  for (auto& [_, refresher_wrapper] : lease_refreshers_) {
    if (refresher_wrapper.lease_refresher_handle->GetLeaseRefreshMode() ==
        LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease) {
      intention_set_count++;
    }
  }

  SCP_INFO(kLeaseManagerV2, activity_id_,
           "Trying to release '%llu' leases. Intention Set Count: '%llu'",
           leases_to_release_count, intention_set_count);

  if (intention_set_count >= leases_to_release_count) {
    // already in the process of release, nothing to do now.
    return;
  }

  size_t effective_leases_to_release_count =
      leases_to_release_count - intention_set_count;
  if (effective_leases_to_release_count == 0) {
    return;
  }

  // Step 2) Set the new ones to release by setting the intention to
  // release. Once the lease is deemed to be released via
  // SafeToReleaseLease(), the lease will be released i.e.
  // RefreshWithNoIntentionToHoldLease will be set on it.
  for (auto& [lock_id, refresher_wrapper] : lease_refreshers_) {
    if (effective_leases_to_release_count == 0) {
      break;
    }
    if (refresher_wrapper.lease_refresher_handle->GetLeaseRefreshMode() ==
            LeaseRefreshMode::RefreshWithIntentionToHoldLease &&
        refresher_wrapper.leasable_lock->IsCurrentLeaseOwner()) {
      SCP_INFO(kLeaseManagerV2, activity_id_,
               "Setting 'RefreshWithIntentionToReleaseTheHeldLease' on the "
               "Lock: '%s'",
               ToString(lock_id).c_str());

      // TRANSITION:
      // RefreshWithIntentionToHoldLease ==>>>
      // RefreshWithIntentionToReleaseTheHeldLease
      auto execution_result =
          refresher_wrapper.lease_refresher_handle->SetLeaseRefreshMode(
              LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease);
      if (!execution_result.Successful()) {
        SCP_ERROR(kLeaseManagerV2, activity_id_, execution_result,
                  "Setting 'RefreshWithIntentionToReleaseTheHeldLease' on the "
                  "Lock: '%s' "
                  "failed. Will retry in the next round.",
                  ToString(lock_id).c_str());
        continue;
      }

      effective_leases_to_release_count--;
    }
  }
}

void LeaseManagerV2::
    RevertTheIntentionToHoldLeaseForLocksWhichCannotBeLeased() {
  // Move back the ones that cannot be leased and changing the leases
  // to hold count accordingly.
  for (auto& [lock_id, refresher_wrapper] : lease_refreshers_) {
    bool lease_already_held_by_someone =
        refresher_wrapper.leasable_lock->GetCurrentLeaseOwnerInfo()
            .has_value() &&
        !refresher_wrapper.leasable_lock->IsCurrentLeaseOwner();
    if ((refresher_wrapper.lease_refresher_handle->GetLeaseRefreshMode() ==
         LeaseRefreshMode::RefreshWithIntentionToHoldLease) &&
        lease_already_held_by_someone) {
      SCP_INFO(kLeaseManagerV2, activity_id_,
               "Reverting 'RefreshWithIntentionToHoldLease' on the Lock: '%s' "
               "to 'RefreshWithNoIntentionToHoldLease'",
               ToString(lock_id).c_str());

      // TRANSITION:
      // RefreshWithIntentionToHoldLease ====>>>
      // RefreshWithNoIntentionToHoldLease
      auto execution_result =
          refresher_wrapper.lease_refresher_handle->SetLeaseRefreshMode(
              LeaseRefreshMode::RefreshWithNoIntentionToHoldLease);
      if (!execution_result.Successful()) {
        SCP_ERROR(
            kLeaseManagerV2, activity_id_, execution_result,
            "Reverting 'RefreshWithIntentionToHoldLease' on the Lock: '%s' "
            "to 'RefreshWithNoIntentionToHoldLease'"
            " failed. Will retry in the next round.",
            ToString(lock_id).c_str());
      }
    }
  }
}

void LeaseManagerV2::PerformLeaseAcquisitionPreferenceManagement() {
  LeaseAcquisitionPreference lease_preference_snapshot;
  {
    unique_lock<mutex> lock(lease_preference_mutex_);
    lease_preference_snapshot = lease_acquisition_preference_;
  }
  unique_lock<mutex> lock(lease_preference_manager_mutex_);
  // Lease new leases (or) give up existing leases
  // 1. Get leases held count.
  size_t leases_held_count = 0;
  for (auto& [_, refresher_wrapper] : lease_refreshers_) {
    if (refresher_wrapper.leasable_lock->IsCurrentLeaseOwner()) {
      leases_held_count++;
    }
  }

  SCP_INFO(kLeaseManagerV2, activity_id_,
           "Performing Lease Preference Management: Leases To Hold Count: "
           "'%llu'. Leases Held Count: '%llu'",
           lease_preference_snapshot.maximum_number_of_leases_to_hold,
           leases_held_count);

  // 2. Try leases hold/release.
  if (leases_held_count <
      lease_preference_snapshot.maximum_number_of_leases_to_hold) {
    TryHoldMoreLeases(
        lease_preference_snapshot.maximum_number_of_leases_to_hold -
        leases_held_count);
  } else if (leases_held_count >
             lease_preference_snapshot.maximum_number_of_leases_to_hold) {
    // Should release some leases.
    TryReleaseSomeLeases(
        leases_held_count -
        lease_preference_snapshot.maximum_number_of_leases_to_hold);
  } else {
    // Lease count has met the desired count.
  }

  // 3. After the above adjustments, we need to change the intention to hold
  // lease for the locks (if any) for which leases cannot be held i.e. some
  // other actor is already holding a lease on them
  RevertTheIntentionToHoldLeaseForLocksWhichCannotBeLeased();
}

void LeaseManagerV2::LeaseAcquisitionPreferenceManagerThreadFunction() {
  while (is_running_) {
    PerformLeaseAcquisitionPreferenceManagement();  // Ignore error
    sleep_for(kLeaseAcquisitionPreferenceInvocationIntervalInMilliseconds);
  }
}

size_t LeaseManagerV2::GetCurrentlyLeasedLocksCount() noexcept {
  size_t leased_count = 0;
  for (auto& [_, refresher_wrapper] : lease_refreshers_) {
    if (refresher_wrapper.leasable_lock->GetCurrentLeaseOwnerInfo()
            .has_value()) {
      leased_count++;
    }
  }
  return leased_count;
}
}  // namespace google::scp::core
