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

#include "partition_lease_preference_applier.h"

#include <cmath>
#include <memory>
#include <thread>

#include "core/common/global_logger/src/global_logger.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LeaseAcquisitionPreference;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::Uuid;
using std::make_unique;
using std::thread;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;

static constexpr char kPartitionLeasePreferenceApplier[] =
    "PartitionLeasePreferenceApplier";

static constexpr milliseconds kDefaultApplierIntervalInMilliseconds =
    seconds(2);

static constexpr seconds kDefaultStartupWaitIntervalInMilliseconds =
    seconds(10);

namespace google::scp::pbs {

PartitionLeasePreferenceApplier::PartitionLeasePreferenceApplier(
    size_t partition_count,
    std::shared_ptr<core::LeaseStatisticsInterface> virtual_node_lease_stats,
    std::shared_ptr<core::LeaseAcquisitionPreferenceInterface>
        partition_lease_acquisition_preference)
    : partition_count_(partition_count),
      virtual_node_lease_stats_(virtual_node_lease_stats),
      partition_lease_acquisition_preference_(
          partition_lease_acquisition_preference),
      is_running_(false),
      object_activity_id_(Uuid::GenerateUuid()) {}

void PartitionLeasePreferenceApplier::ThreadFunction() {
  sleep_for(kDefaultStartupWaitIntervalInMilliseconds);
  while (true) {
    if (!is_running_) {
      return;
    }
    auto execution_result = ApplyLeasePreference();
    if (!execution_result.Successful()) {
      SCP_ERROR(kPartitionLeasePreferenceApplier, object_activity_id_,
                execution_result,
                "Failed to apply lease preference. Will Retry later.");
    }
    sleep_for(kDefaultApplierIntervalInMilliseconds);
  }
}

ExecutionResult PartitionLeasePreferenceApplier::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PartitionLeasePreferenceApplier::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_LEASE_PREF_APPLIER_ALREADY_RUNNING);
  }
  is_running_ = true;

  // Start and wait until it starts.
  std::atomic<bool> is_thread_started(false);
  worker_thread_ = make_unique<thread>([this, &is_thread_started]() {
    is_thread_started = true;
    ThreadFunction();
  });
  while (!is_thread_started) {
    sleep_for(milliseconds(100));
  }

  return SuccessExecutionResult();
}

ExecutionResult PartitionLeasePreferenceApplier::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_LEASE_PREF_APPLIER_NOT_RUNNING);
  }
  is_running_ = false;
  if (worker_thread_->joinable()) {
    worker_thread_->join();
  }
  return SuccessExecutionResult();
}

ExecutionResult
PartitionLeasePreferenceApplier::ApplyLeasePreference() noexcept {
  size_t num_vnode_leases_held =
      virtual_node_lease_stats_->GetCurrentlyLeasedLocksCount();

  if (num_vnode_leases_held == 0) {
    auto execution_result = FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_LEASE_PREF_APPLIER_NO_OP);
    SCP_ERROR(kPartitionLeasePreferenceApplier, object_activity_id_,
              execution_result, "Number of VNode Leases Held is zero");
    return execution_result;
  }

  size_t num_partitions_to_hold = static_cast<size_t>(
      std::ceil(partition_count_ / static_cast<double>(num_vnode_leases_held)));

  SCP_INFO(kPartitionLeasePreferenceApplier, object_activity_id_,
           "Partition Count: '%llu', Number of VNode Leases Held: '%llu', "
           "Number of Partitions to hold: '%llu'",
           partition_count_, num_vnode_leases_held, num_partitions_to_hold);

  return partition_lease_acquisition_preference_->SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{num_partitions_to_hold,
                                 {} /* no specific locks to hold */});
}

}  // namespace google::scp::pbs
