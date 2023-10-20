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

#include "lease_refresher.h"

#include <memory>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"

#include "error_codes.h"

using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using std::make_unique;
using std::optional;
using std::thread;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;

static constexpr char kLeaseRefresher[] = "LeaseRefresher";
static constexpr milliseconds kLeaseRefreshInvocationIntervalInMilliseconds =
    milliseconds(1000);

namespace google::scp::core {
LeaseRefresher::LeaseRefresher(
    const LeasableLockId& leasable_lock_id,
    const std::shared_ptr<LeasableLockInterface>& leasable_lock,
    const std::shared_ptr<LeaseEventSinkInterface>& lease_event_sink)
    : leasable_lock_(leasable_lock),
      lease_event_sink_(lease_event_sink),
      prev_lease_refresh_mode_(LeaseRefreshMode::Unknown),
      lease_refresh_mode_(LeaseRefreshMode::RefreshWithNoIntentionToHoldLease),
      last_lease_refresh_timestamp_(
          TimeProvider::GetSteadyTimestampInNanoseconds()),
      is_running_(false),
      was_lease_owner_(false),
      leasable_lock_id_(leasable_lock_id),
      object_activity_id_(Uuid::GenerateUuid()) {}

ExecutionResult LeaseRefresher::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult LeaseRefresher::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(errors::SC_LEASE_REFRESHER_ALREADY_RUNNING);
  }
  is_running_ = true;
  // Initiate start on thread and wait until it starts.
  std::atomic<bool> is_thread_started(false);
  lease_refresher_thread_ = make_unique<thread>([this, &is_thread_started]() {
    is_thread_started = true;
    LeaseRefreshThreadFunction();
  });
  while (!is_thread_started) {
    sleep_for(milliseconds(100));
  }
  return SuccessExecutionResult();
}

ExecutionResult LeaseRefresher::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_LEASE_REFRESHER_NOT_RUNNING);
  }
  is_running_ = false;
  if (lease_refresher_thread_->joinable()) {
    lease_refresher_thread_->join();
  }
  return SuccessExecutionResult();
}

LeaseRefreshMode LeaseRefresher::GetLeaseRefreshMode() const noexcept {
  SCP_DEBUG(kLeaseRefresher, object_activity_id_,
            "LockId: '%s' Returning LeaseRefreshMode as '%llu'",
            ToString(leasable_lock_id_).c_str(), lease_refresh_mode_.load());
  return lease_refresh_mode_.load();
}

/**
 * @brief Helper function to perform a Lease Transition and Notify the lease
 * event sink about the Lease Transition
 *
 * @param leasable_lock_id lock ID
 * @param lease_event_sink the sink object where the notification will be sent
 * @param is_lease_owner am I the lease owner in the current round
 * @param lease_refresh_mode the mode of lease refresh
 * @param was_lease_owner was I the lease owner in previous round
 * @param current_lease_owner_info lease owner of the current round
 * @param prev_lease_refresh_mode the prev mode of lease refresh
 *
 * @return LeaseTransitionType returns the performed Lease Transition type.
 */
static LeaseTransitionType PerformLeaseTransitionAndNotifyLeaseEventSink(
    const LeasableLockId& leasable_lock_id,
    std::shared_ptr<LeaseEventSinkInterface> lease_event_sink,
    bool is_lease_owner, LeaseRefreshMode lease_refresh_mode,
    bool was_lease_owner, optional<LeaseInfo> current_lease_owner_info,
    LeaseRefreshMode prev_lease_refresh_mode) {
  if (!was_lease_owner && is_lease_owner) {
    // 1. Previously NOT owned, and currently owned.
    lease_event_sink->OnLeaseTransition(leasable_lock_id,
                                        LeaseTransitionType::kAcquired,
                                        current_lease_owner_info);
    return LeaseTransitionType::kAcquired;
  } else if (was_lease_owner && !is_lease_owner) {
    // 2. Previously owned, and currently NOT owned.
    // Send a lease release event (kReleased) instead of lease lost event
    // (kLost) if the mode transitions from
    // RefreshWithIntentionToReleaseTheHeldLease.
    bool should_send_release_notification =
        (prev_lease_refresh_mode ==
         LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease);
    if (should_send_release_notification) {
      lease_event_sink->OnLeaseTransition(leasable_lock_id,
                                          LeaseTransitionType::kReleased,
                                          current_lease_owner_info);
      return LeaseTransitionType::kReleased;
    } else {
      lease_event_sink->OnLeaseTransition(leasable_lock_id,
                                          LeaseTransitionType::kLost,
                                          current_lease_owner_info);
      return LeaseTransitionType::kLost;
    }
  } else if (was_lease_owner && is_lease_owner) {
    // 3. Previously and currently owned.
    bool should_send_release_notification =
        (lease_refresh_mode ==
         LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease);
    if (should_send_release_notification) {
      lease_event_sink->OnLeaseTransition(
          leasable_lock_id, LeaseTransitionType::kRenewedWithIntentionToRelease,
          current_lease_owner_info);
      return LeaseTransitionType::kRenewedWithIntentionToRelease;
    } else {
      lease_event_sink->OnLeaseTransition(leasable_lock_id,
                                          LeaseTransitionType::kRenewed,
                                          current_lease_owner_info);
      return LeaseTransitionType::kRenewed;
    }
  } else if (!was_lease_owner && !is_lease_owner) {
    // 4. Previously NOT owned and currently NOT owned.
    lease_event_sink->OnLeaseTransition(leasable_lock_id,
                                        LeaseTransitionType::kNotAcquired,
                                        current_lease_owner_info);
    return LeaseTransitionType::kNotAcquired;
  } else {
    // This should never happen! Keeping the compiler happy :)
    SCP_EMERGENCY(kLeaseRefresher, kZeroUuid,
                  FailureExecutionResult(SC_UNKNOWN),
                  "Unknown state transition");
    std::abort();
  }
}

ExecutionResult LeaseRefresher::PerformLeaseRefresh() noexcept {
  std::unique_lock lock(lease_refresh_mutex_);
  auto execution_result = SuccessExecutionResult();
  auto refresh_start_timestamp = duration_cast<milliseconds>(
      TimeProvider::GetSteadyTimestampInNanoseconds());

  //
  // 1) Refresh Lease
  //
  bool perform_lease_refresh = leasable_lock_->ShouldRefreshLease();
  if (perform_lease_refresh) {
    bool is_lease_refresh_read_only =
        (lease_refresh_mode_ ==
         LeaseRefreshMode::RefreshWithNoIntentionToHoldLease);
    execution_result = leasable_lock_->RefreshLease(is_lease_refresh_read_only);
    if (!execution_result.Successful()) {
      SCP_ERROR(kLeaseRefresher, object_activity_id_, execution_result,
                "Cannot refresh lease");
      // Continue with notifying the sink.
    }
  }
  //
  // 2) Run State Machine and Notify Lease Event Sink (if needed)
  //
  bool was_lease_owner = was_lease_owner_;
  auto prev_lease_refresh_mode = prev_lease_refresh_mode_.load();
  bool is_lease_owner = leasable_lock_->IsCurrentLeaseOwner();
  auto lease_owner_info = leasable_lock_->GetCurrentLeaseOwnerInfo();
  auto lease_refresh_mode = lease_refresh_mode_.load();
  auto lease_event_sink = lease_event_sink_.lock();
  if (lease_event_sink && perform_lease_refresh) {
    last_lease_transition_ = PerformLeaseTransitionAndNotifyLeaseEventSink(
        leasable_lock_id_, lease_event_sink, is_lease_owner, lease_refresh_mode,
        was_lease_owner, lease_owner_info, prev_lease_refresh_mode);
  }
  was_lease_owner_ = is_lease_owner;
  //
  // 3) Update lease refresh timestamp.
  //
  last_lease_refresh_timestamp_ =
      TimeProvider::GetSteadyTimestampInNanoseconds();

  SCP_INFO(
      kLeaseRefresher, object_activity_id_,
      "Lease Refreshed at '%llu'. LockId: '%s', WasLeaseOwner: '%d', "
      "IsLeaseOwnerNow: '%d', "
      "LeaseRefreshMode: '%d', PrevLeaseRefreshMode: '%d', HasLeaseOwner: '%d'"
      "Elapsed Time (ms): '%llu'",
      last_lease_refresh_timestamp_.load().count(),
      ToString(leasable_lock_id_).c_str(), was_lease_owner, is_lease_owner,
      lease_refresh_mode, prev_lease_refresh_mode, lease_owner_info.has_value(),
      ((duration_cast<milliseconds>(last_lease_refresh_timestamp_.load()) -
        refresh_start_timestamp))
          .count());

  return execution_result;
}

void LeaseRefresher::LeaseRefreshThreadFunction() {
  while (is_running_) {
    PerformLeaseRefresh();  // Ignore error
    sleep_for(kLeaseRefreshInvocationIntervalInMilliseconds);
  }
}

ExecutionResult LeaseRefresher::SetLeaseRefreshMode(
    LeaseRefreshMode lease_refresh_mode) noexcept {
  // Lock is taken to protect an on-going lease refresh with the setting of
  // lease refresh mode.
  std::unique_lock lock(lease_refresh_mutex_);
  SCP_INFO(
      kLeaseRefresher, object_activity_id_,
      "LockId: '%s' Setting Lease Refresh Mode to: '%d', Current Mode: '%d'",
      ToString(leasable_lock_id_).c_str(), lease_refresh_mode,
      lease_refresh_mode_.load());

  // Check if the lease transition is safe to perform the
  // 'RefreshWithIntentionToHoldLease' change. Lease refresher should have sent
  // the 'kReleased' or 'kLost' and the transition should settle at
  // 'kNotAcquired'. If not, we should not allow the mode change to avoid losing
  // a lease transition of 'kReleased' or 'kLost'.
  if (last_lease_transition_.has_value() &&
      (*last_lease_transition_ != LeaseTransitionType::kNotAcquired) &&
      (lease_refresh_mode ==
       LeaseRefreshMode::RefreshWithIntentionToHoldLease)) {
    return FailureExecutionResult(
        errors::SC_LEASE_REFRESHER_INVALID_STATE_WHEN_SETTING_REFRESH_MODE);
  }

  prev_lease_refresh_mode_ = lease_refresh_mode_.load();
  lease_refresh_mode_ = lease_refresh_mode;

  return SuccessExecutionResult();
}

nanoseconds LeaseRefresher::GetLastLeaseRefreshTimestamp() const noexcept {
  auto timestamp = last_lease_refresh_timestamp_.load();
  SCP_INFO(kLeaseRefresher, object_activity_id_,
           "LockId: '%s' Returning '%llu'", ToString(leasable_lock_id_).c_str(),
           timestamp);
  return timestamp;
}
}  // namespace google::scp::core
