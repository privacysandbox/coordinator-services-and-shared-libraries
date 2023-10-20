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
#include "lease_manager.h"

#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using std::abort;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::mutex;
using std::shared_ptr;
using std::thread;
using std::unique_lock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::core {
static constexpr milliseconds kNoOngoingLeaseAcquisitionTimestamp(0);
static constexpr milliseconds kSleepDurationMs(10);

static constexpr char kLeaseManager[] = "LeaseManager";

LeaseManager::LeaseManager(
    TimeDuration lease_enforcer_frequency_in_milliseconds,
    TimeDuration lease_obtainer_maximum_runningtime_in_milliseconds)
    : lease_enforcer_thread_started_(false),
      lease_obtainer_thread_started_(false),
      obtain_lease_(false),
      component_running_(false),
      ongoing_lease_acquisition_start_timestamp_(
          kNoOngoingLeaseAcquisitionTimestamp),
      terminate_process_function_([]() {
        SCP_EMERGENCY(kLeaseManager, kZeroUuid,
                      FailureExecutionResult(SC_UNKNOWN),
                      "Terminating process..");
        abort();
      }),
      lease_enforcer_frequency_in_milliseconds_(
          lease_enforcer_frequency_in_milliseconds),
      lease_obtainer_maximum_runningtime_in_milliseconds_(
          lease_obtainer_maximum_runningtime_in_milliseconds) {}

ExecutionResult LeaseManager::ManageLeaseOnLock(
    shared_ptr<LeasableLockInterface> leasable_lock,
    LeaseTransitionCallback&& lease_transition_callback) noexcept {
  if (component_running_) {
    // More than one lock is not supported yet
    return FailureExecutionResult(
        errors::SC_LEASE_MANAGER_LOCK_CANNOT_BE_ADDED_WHILE_RUNNING);
  }

  // Lease duration must be atleast a couple of cycles of lease enforcer
  // thread enforcement period.
  if (leasable_lock->GetConfiguredLeaseDurationInMilliseconds() <
      ((uint64_t)(2 * lease_enforcer_frequency_in_milliseconds_.count()))) {
    return FailureExecutionResult(
        errors::SC_LEASE_MANAGER_LOCK_LEASE_DURATION_INVALID);
  }

  leasable_lock_ = leasable_lock;
  lease_transition_callback_ = move(lease_transition_callback);
  return SuccessExecutionResult();
}

void LeaseManager::LeaseObtainerThreadFunction() {
  bool previously_lease_owner = false;
  unique_lock<mutex> lock(lease_obtainer_thread_mutex_);
  while (true) {
    obtain_lease_condvar_.wait(
        lock, [&]() { return obtain_lease_ || !component_running_; });

    if (!component_running_) {
      break;
    }
    obtain_lease_ = false;
    lock.unlock();

    ongoing_lease_acquisition_start_timestamp_ =
        TimeProvider::GetSteadyTimestampInNanoseconds();

    auto is_read_only_lease_refresh = false;
    auto result = leasable_lock_->RefreshLease(is_read_only_lease_refresh);
    if (!result.Successful()) {
      SCP_ERROR(kLeaseManager, kZeroUuid, result,
                "Unable to refresh lease on the lock.");
    }

    auto is_current_lease_owner = leasable_lock_->IsCurrentLeaseOwner();
    auto lease_owner_info = leasable_lock_->GetCurrentLeaseOwnerInfo();
    if (!previously_lease_owner && is_current_lease_owner) {
      lease_transition_callback_(LeaseTransitionType::kAcquired,
                                 lease_owner_info);
    } else if (previously_lease_owner && !is_current_lease_owner) {
      lease_transition_callback_(LeaseTransitionType::kLost, lease_owner_info);
    } else if (previously_lease_owner && is_current_lease_owner) {
      lease_transition_callback_(LeaseTransitionType::kRenewed,
                                 lease_owner_info);
    } else {
      lease_transition_callback_(LeaseTransitionType::kNotAcquired,
                                 lease_owner_info);
    }
    previously_lease_owner = is_current_lease_owner;
    ongoing_lease_acquisition_start_timestamp_ =
        kNoOngoingLeaseAcquisitionTimestamp;

    lock.lock();
  }
}

void LeaseManager::LeaseEnforcerThreadFunction() {
  unique_lock<mutex> lock(lease_enforcer_thread_mutex_);
  while (true) {
    component_running_condvar_.wait_for(
        lock, lease_enforcer_frequency_in_milliseconds_,
        [&]() { return !component_running_; });

    if (!component_running_) {
      NotifyLeaseObtainer(false /* obtain lease */);
      break;
    }

    if (leasable_lock_->ShouldRefreshLease()) {
      NotifyLeaseObtainer(true /* obtain lease */);
    }

    // TODO b/280466468 Ensure lease obtainer is acknowledging
    // notification, otherwise terminate the process.

    if (IsLeaseRefreshTakingLong()) {
      terminate_process_function_();
    }
  }
}

void LeaseManager::NotifyLeaseObtainer(bool obtain_lease) {
  unique_lock<mutex> lock(lease_obtainer_thread_mutex_);
  obtain_lease_ = obtain_lease;
  obtain_lease_condvar_.notify_all();
}

void LeaseManager::NotifyLeaseEnforcerToStopRunning() {
  unique_lock<mutex> lock(lease_enforcer_thread_mutex_);
  component_running_ = false;
  component_running_condvar_.notify_all();
}

bool LeaseManager::IsLeaseRefreshTakingLong() {
  auto ongoing_lease_acquisition_start_timestamp =
      ongoing_lease_acquisition_start_timestamp_.load();
  auto now_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();
  if (ongoing_lease_acquisition_start_timestamp ==
      kNoOngoingLeaseAcquisitionTimestamp) {
    return false;
  }
  auto is_lease_refresh_taking_long =
      (now_timestamp - ongoing_lease_acquisition_start_timestamp >
       lease_obtainer_maximum_runningtime_in_milliseconds_);
  if (is_lease_refresh_taking_long) {
    SCP_INFO(kLeaseManager, kZeroUuid,
             "Lease refresh took too long. Time taken: %lld",
             (now_timestamp - ongoing_lease_acquisition_start_timestamp));
  }
  return is_lease_refresh_taking_long;
}

ExecutionResult LeaseManager::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult LeaseManager::Run() noexcept {
  if (!leasable_lock_ || !lease_transition_callback_) {
    // Leasable lock must be provided before running the Lease Manager
    return FailureExecutionResult(errors::SC_LEASE_MANAGER_NOT_INITIALIZED);
  }

  if (component_running_) {
    return FailureExecutionResult(errors::SC_LEASE_MANAGER_ALREADY_RUNNING);
  }

  component_running_ = true;

  lease_obtainer_thread_ = make_unique<thread>([this]() {
    lease_obtainer_thread_started_ = true;
    LeaseObtainerThreadFunction();
  });
  while (!lease_obtainer_thread_started_) {
    sleep_for(milliseconds(kSleepDurationMs));
  }

  lease_enforcer_thread_ = make_unique<thread>([this]() {
    lease_enforcer_thread_started_ = true;
    LeaseEnforcerThreadFunction();
  });
  while (!lease_enforcer_thread_started_) {
    sleep_for(milliseconds(kSleepDurationMs));
  }

  // TODO: Set high priority for threads.
  return SuccessExecutionResult();
}

ExecutionResult LeaseManager::Stop() noexcept {
  if (!component_running_) {
    return FailureExecutionResult(errors::SC_LEASE_MANAGER_NOT_RUNNING);
  }

  NotifyLeaseEnforcerToStopRunning();

  if (lease_enforcer_thread_->joinable()) {
    lease_enforcer_thread_->join();
    lease_enforcer_thread_started_ = false;
  }

  if (lease_obtainer_thread_->joinable()) {
    lease_obtainer_thread_->join();
    lease_obtainer_thread_started_ = false;
  }

  return SuccessExecutionResult();
}
};  // namespace google::scp::core
