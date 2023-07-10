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

#include "pbs/leasable_lock/src/leasable_lock_on_nosql_database.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>

#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "pbs/leasable_lock/src/error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManagerInterface;
using google::scp::core::LeaseTransitionType;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using std::atomic;
using std::get;
using std::make_shared;
using std::mutex;
using std::optional;
using std::shared_lock;
using std::shared_mutex;
using std::shared_ptr;
using std::string;
using std::unique_lock;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::pbs {

constexpr char kLeaseManager[] = "LeaseManager";

// NOTE: This leasable lock implementation assumes a row already exists on
// the database for the specified 'lock_row_key_' in the constructor
// arguments.
LeasableLockOnNoSQLDatabase::LeasableLockOnNoSQLDatabase(
    shared_ptr<NoSQLDatabaseProviderInterface> database,
    LeaseInfo lease_acquirer_info, string table_name, string lock_row_key,
    milliseconds lease_duration_in_milliseconds,
    uint64_t lease_renewal_threshold_percent_time_left_in_lease) noexcept
    : database_(database),
      lease_acquirer_info_(lease_acquirer_info),
      table_name_(table_name),
      lock_row_key_(lock_row_key),
      lease_duration_in_milliseconds_(lease_duration_in_milliseconds),
      lease_renewal_threshold_percent_time_left_in_lease_(
          lease_renewal_threshold_percent_time_left_in_lease) {}

ExecutionResult LeasableLockOnNoSQLDatabase::RefreshLease() noexcept {
  unique_lock<mutex> lock(mutex_);

  LeaseInfoInternal lease_read;
  auto result = ReadLeaseSynchronouslyFromDatabase(lease_read);
  if (!result.Successful()) {
    // TODO: kZeroUuid for now. Add an activity argument to
    // RefreshLease so that the caller's activity can be used here.
    ERROR(kLeaseManager, kZeroUuid, kZeroUuid, result,
          "Failed to read lease from the database.");
    return result;
  }

  if (lease_read.lease_acquisition_disallowed) {
    return core::FailureExecutionResult(
        core::errors::SC_LEASABLE_LOCK_ACQUISITION_DISALLOWED);
  }

  if (!lease_read.IsLeaseOwner(lease_acquirer_info_.lease_acquirer_id) &&
      !lease_read.IsExpired()) {
    current_lease_ = lease_read;
    return result;
  }

  // Renew(if owner) or Acquire(if not owner) the lease
  LeaseInfoInternal new_lease = lease_read;
  if (!lease_read.IsLeaseOwner(lease_acquirer_info_.lease_acquirer_id)) {
    new_lease = LeaseInfoInternal(lease_acquirer_info_);
  }
  new_lease.ExtendLeaseDurationInMillisecondsFromCurrentTimestamp(
      lease_duration_in_milliseconds_);
  result = WriteLeaseSynchronouslyToDatabase(lease_read, new_lease);
  if (!result.Successful()) {
    // TODO: kZeroUuid for now. Add an activity argument to
    // RefreshLease so that the caller's activity can be used here.
    ERROR(kLeaseManager, kZeroUuid, kZeroUuid, result,
          "Failed to update lease on the database.");
    return result;
  }

  current_lease_ = new_lease;
  return result;
}

bool LeasableLockOnNoSQLDatabase::ShouldRefreshLease() const noexcept {
  unique_lock<mutex> lock(mutex_);
  // Lease will be refreshed if
  // 1. Current Lease is expired
  // 2. Current Lease is not expired, the lease is owned by this caller and
  // lease renew threshold has been reached.
  if (!current_lease_.has_value()) {
    return true;
  } else if (current_lease_->IsExpired()) {
    return true;
  } else if (current_lease_->IsLeaseOwner(
                 lease_acquirer_info_.lease_acquirer_id)) {
    return current_lease_->IsLeaseRenewalRequired(
        lease_duration_in_milliseconds_,
        lease_renewal_threshold_percent_time_left_in_lease_);
  }
  return false;
}

TimeDuration
LeasableLockOnNoSQLDatabase::GetConfiguredLeaseDurationInMilliseconds()
    const noexcept {
  return lease_duration_in_milliseconds_.count();
}

optional<LeaseInfo> LeasableLockOnNoSQLDatabase::GetCurrentLeaseOwnerInfo()
    const noexcept {
  unique_lock<mutex> lock(mutex_);
  // If current cached lease info says that the lease is expired, then do not
  // return stale information.
  if (current_lease_.has_value() && !current_lease_->IsExpired()) {
    return current_lease_->lease_owner_info;
  }
  return {};
}

bool LeasableLockOnNoSQLDatabase::IsCurrentLeaseOwner() const noexcept {
  // If cached lease info is expired, assume lease is lost (if the caller was
  // an owner of the lease).
  unique_lock<mutex> lock(mutex_);
  return current_lease_.has_value() && !current_lease_->IsExpired() &&
         current_lease_->IsLeaseOwner(lease_acquirer_info_.lease_acquirer_id);
}

bool LeasableLockOnNoSQLDatabase::LeaseInfoInternal::IsExpired() const {
  auto now_timestamp_in_milliseconds = GetCurrentTimestampInMilliseconds();
  return now_timestamp_in_milliseconds >
         lease_expiraton_timestamp_in_milliseconds;
}

bool LeasableLockOnNoSQLDatabase::LeaseInfoInternal::IsLeaseOwner(
    string lock_acquirer_id) const {
  return lease_owner_info.lease_acquirer_id == lock_acquirer_id;
}

void LeasableLockOnNoSQLDatabase::LeaseInfoInternal::
    ExtendLeaseDurationInMillisecondsFromCurrentTimestamp(
        milliseconds lease_duration_in_milliseconds) {
  lease_expiraton_timestamp_in_milliseconds =
      GetCurrentTimestampInMilliseconds() + lease_duration_in_milliseconds;
}

bool LeasableLockOnNoSQLDatabase::LeaseInfoInternal::IsLeaseRenewalRequired(
    milliseconds lease_duration_in_milliseconds,
    uint64_t lease_renewal_threshold_percent_time_left_in_lease) const {
  auto now_timestamp_in_milliseconds = GetCurrentTimestampInMilliseconds();
  auto time_left_in_lease_in_milliseconds =
      lease_expiraton_timestamp_in_milliseconds - now_timestamp_in_milliseconds;
  double percent_time_left =
      (time_left_in_lease_in_milliseconds.count() * 100) /
      lease_duration_in_milliseconds.count();
  return (percent_time_left <
          lease_renewal_threshold_percent_time_left_in_lease);
}

milliseconds LeasableLockOnNoSQLDatabase::LeaseInfoInternal::
    GetCurrentTimestampInMilliseconds() const {
  return duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds());
}
};  // namespace google::scp::pbs
