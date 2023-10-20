
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

#include "lease_refresh_liveness_enforcer.h"

#include <pthread.h>
#include <sched.h>

#include <memory>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"

#include "error_codes.h"

using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using std::make_shared;
using std::make_unique;
using std::thread;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

static constexpr milliseconds kDefaultEnforcerIntervalInMilliseconds =
    milliseconds(1000);

static constexpr char kLeaseRefreshLivenessEnforcer[] =
    "LeaseRefreshLivenessEnforcer";

namespace google::scp::core {

LeaseRefreshLivenessEnforcer::LeaseRefreshLivenessEnforcer(
    const std::vector<std::shared_ptr<LeaseRefreshLivenessCheckInterface>>&
        lease_refreshers,
    std::chrono::milliseconds lease_duration_in_milliseconds,
    std::function<void()> corrective_action_for_liveness)
    : lease_refresher_list_(lease_refreshers),
      lease_duration_in_milliseconds_(lease_duration_in_milliseconds),
      enforcement_interval_in_milliseconds_(
          kDefaultEnforcerIntervalInMilliseconds),
      is_running_(false),
      corrective_action_for_liveness_(corrective_action_for_liveness),
      object_activity_id_(Uuid::GenerateUuid()) {}

ExecutionResult
LeaseRefreshLivenessEnforcer::PerformLeaseLivenessEnforcement() noexcept {
  std::unique_lock lock(lease_enforcer_mutex_);
  for (auto& lease_refresher : lease_refresher_list_) {
    auto lease_refresh_steady_timestamp =
        lease_refresher->GetLastLeaseRefreshTimestamp();
    auto current_steady_timestamp =
        TimeProvider::GetSteadyTimestampInNanoseconds();
    auto elapsed_time =
        current_steady_timestamp - lease_refresh_steady_timestamp;
    if (elapsed_time > lease_duration_in_milliseconds_) {
      auto execution_result = FailureExecutionResult(
          errors::SC_LEASE_LIVENESS_ENFORCER_LIVENESS_VIOLATION);
      SCP_ERROR(
          kLeaseRefreshLivenessEnforcer, object_activity_id_, execution_result,
          "Lease Refresh Steady Timestamp: '%llu', Current Steady Timestamp: "
          "'%llu', Elapsed Time: '%llu' (ms), Lease Duration: '%llu'",
          lease_refresh_steady_timestamp.count(),
          current_steady_timestamp.count(),
          duration_cast<milliseconds>(elapsed_time).count(),
          lease_duration_in_milliseconds_.count());
      return execution_result;
    }
  }
  return SuccessExecutionResult();
}

void LeaseRefreshLivenessEnforcer::LivenessEnforcerThreadFunction() {
  while (true) {
    if (!is_running_) {
      SCP_INFO(kLeaseRefreshLivenessEnforcer, object_activity_id_,
               "Exiting LivenessEnforcerThreadFunction because the component "
               "is not running.");
      return;
    }
    // Enforce
    auto start_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();
    auto execution_result = PerformLeaseLivenessEnforcement();
    if (!execution_result.Successful()) {
      SCP_EMERGENCY(kLeaseRefreshLivenessEnforcer, object_activity_id_,
                    execution_result, "Terminating!");
      // This is a simple std::abort of the whole process. This can be a
      // specific action to a lease refresher to restart or something that can
      // be incrementally added if necessary.
      corrective_action_for_liveness_();
    }
    auto end_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();

    // Sleep until next round, only the required amount.
    auto enforcement_elapsed_duration_in_milliseconds =
        duration_cast<milliseconds>(end_timestamp - start_timestamp);
    auto sleep_duration_in_milliseconds = milliseconds(0);
    if (enforcement_elapsed_duration_in_milliseconds <
        enforcement_interval_in_milliseconds_) {
      sleep_duration_in_milliseconds =
          enforcement_interval_in_milliseconds_ -
          enforcement_elapsed_duration_in_milliseconds;
    }

    SCP_DEBUG(kLeaseRefreshLivenessEnforcer, object_activity_id_,
              "Completed enforcement round. Time elapsed: '%llu' (ms). "
              "Sleeping for '%llu' (ms) until the next round",
              enforcement_elapsed_duration_in_milliseconds.count(),
              sleep_duration_in_milliseconds.count());

    sleep_for(sleep_duration_in_milliseconds);
  }
}

ExecutionResult LeaseRefreshLivenessEnforcer::Init() noexcept {
  if (lease_duration_in_milliseconds_ <
      (2 * enforcement_interval_in_milliseconds_)) {
    return FailureExecutionResult(
        errors::SC_LEASE_LIVENESS_ENFORCER_INSUFFICIENT_PERIOD);
  }
  return SuccessExecutionResult();
}

ExecutionResult LeaseRefreshLivenessEnforcer::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        errors::SC_LEASE_LIVENESS_ENFORCER_ALREADY_RUNNING);
  }
  is_running_ = true;

  // Start Enforcer and wait until it starts.
  std::atomic<bool> is_thread_started(false);
  enforcer_thread_ = make_unique<thread>([this, &is_thread_started]() {
    is_thread_started = true;
    LivenessEnforcerThreadFunction();
  });
  while (!is_thread_started) {
    sleep_for(milliseconds(100));
  }

  // Set a min-priority under FIFO scheduling policy to increase the chances of
  // this thread getting scheduled.
  int priority = sched_get_priority_min(SCHED_FIFO);
  if (priority == -1) {
    SCP_CRITICAL(kLeaseRefreshLivenessEnforcer, object_activity_id_,
                 FailureExecutionResult(
                     core::errors::
                         SC_LEASE_LIVENESS_ENFORCER_CANNOT_SET_THREAD_PRIORITY),
                 "LeaseRefreshLivenessEnforcer sched_get_priority_min returned "
                 "-1, errno: '%d'",
                 errno);
  } else {
    sched_param param{priority};
    int error = pthread_setschedparam(enforcer_thread_->native_handle(),
                                      SCHED_FIFO, &param);
    if (error != 0) {
      SCP_CRITICAL(
          kLeaseRefreshLivenessEnforcer, object_activity_id_,
          FailureExecutionResult(
              core::errors::
                  SC_LEASE_LIVENESS_ENFORCER_CANNOT_SET_THREAD_PRIORITY),
          "LeaseRefreshLivenessEnforcer pthread_setschedparam cannot be set, "
          "error code: '%d'",
          error);
    }
  }

  SCP_INFO(kLeaseRefreshLivenessEnforcer, object_activity_id_,
           "LeaseRefreshLivenessEnforcer ran successfully");

  return SuccessExecutionResult();
}

ExecutionResult LeaseRefreshLivenessEnforcer::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        errors::SC_LEASE_LIVENESS_ENFORCER_NOT_RUNNING);
  }
  is_running_ = false;
  if (enforcer_thread_->joinable()) {
    enforcer_thread_->join();
  }
  SCP_INFO(kLeaseRefreshLivenessEnforcer, object_activity_id_,
           "LeaseRefreshLivenessEnforcer stopped successfully");
  return SuccessExecutionResult();
}
}  // namespace google::scp::core
