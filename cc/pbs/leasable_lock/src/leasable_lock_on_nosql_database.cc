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
using google::scp::core::FailureExecutionResult;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManagerInterface;
using google::scp::core::LeaseTransitionType;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
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

constexpr char kLeasableLock[] = "LeasableLock";

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
          lease_renewal_threshold_percent_time_left_in_lease),
      activity_id_(Uuid::GenerateUuid()) {}

ExecutionResult LeasableLockOnNoSQLDatabase::RefreshLease(
    bool is_read_only_lease_refresh) noexcept {
  unique_lock<mutex> lock(mutex_);

  SCP_INFO(kLeasableLock, activity_id_,
           "LockId: '%s', Starting to refresh the lease.",
           lock_row_key_.c_str());

  LeaseInfoInternal lease_read;
  auto result = ReadLeaseSynchronouslyFromDatabase(lease_read);
  if (!result.Successful()) {
    SCP_ERROR(kLeasableLock, activity_id_, result,
              "LockId: '%s', Failed to read the lease.", lock_row_key_.c_str());
    return result;
  }

  SCP_INFO(kLeasableLock, activity_id_,
           "LockId: '%s', Read the current lease from lock.",
           lock_row_key_.c_str());

  if (lease_read.lease_acquisition_disallowed) {
    auto execution_result = core::FailureExecutionResult(
        core::errors::SC_LEASABLE_LOCK_ACQUISITION_DISALLOWED);
    SCP_ERROR(kLeasableLock, activity_id_, execution_result,
              "LockId: '%s', Lease acquisition is disallowed!",
              lock_row_key_.c_str());
    return execution_result;
  }

  if (!lease_read.IsLeaseOwner(lease_acquirer_info_.lease_acquirer_id) &&
      !lease_read.IsExpired()) {
    current_lease_ = lease_read;
    return result;
  }

  // If lease should not be acquired, i.e. lease is read only, then return with
  // the read lease.
  if (is_read_only_lease_refresh) {
    current_lease_ = lease_read;
    return result;
  }

  SCP_INFO(kLeasableLock, activity_id_,
           "LockId: '%s', Attempting to write new lease to lock.",
           lock_row_key_.c_str());

  // Renew(if owner) or Acquire(if not owner) the lease
  LeaseInfoInternal new_lease = lease_read;
  if (!lease_read.IsLeaseOwner(lease_acquirer_info_.lease_acquirer_id)) {
    new_lease = LeaseInfoInternal(lease_acquirer_info_);
  }
  new_lease.SetExpirationTimestampFromNow(lease_duration_in_milliseconds_);
  result = WriteLeaseSynchronouslyToDatabase(lease_read, new_lease);
  if (!result.Successful()) {
    SCP_ERROR(kLeasableLock, activity_id_, result,
              "LockId: '%s', Failed to update lease on the database. "
              "Expiration Timestamp (ms): '%llu', Remaining time on Lease "
              "(ms): '%lld'",
              lock_row_key_.c_str(),
              lease_read.lease_expiraton_timestamp_in_milliseconds,
              lease_read.lease_expiraton_timestamp_in_milliseconds -
                  duration_cast<milliseconds>(
                      TimeProvider::GetWallTimestampInNanoseconds()));
    return result;
  }

  SCP_INFO(
      kLeasableLock, activity_id_,
      "LockId: '%s', Lease Written. Updated the lease to ID: '%s', Address: "
      "'%s', Expiration "
      "Timestamp (ms): '%llu'. Previous Expiration Timestamp was (ms): '%llu', "
      "Current - Previous Expiration Timestamps (ms): '%lld', Time remaining "
      "on the Lease (ms): '%lld'",
      lock_row_key_.c_str(),
      new_lease.lease_owner_info.lease_acquirer_id.c_str(),
      new_lease.lease_owner_info.service_endpoint_address.c_str(),
      new_lease.lease_expiraton_timestamp_in_milliseconds,
      lease_read.lease_expiraton_timestamp_in_milliseconds,
      new_lease.lease_expiraton_timestamp_in_milliseconds -
          lease_read.lease_expiraton_timestamp_in_milliseconds,
      new_lease.lease_expiraton_timestamp_in_milliseconds -
          duration_cast<milliseconds>(
              TimeProvider::GetWallTimestampInNanoseconds()));

  current_lease_ = new_lease;
  return result;
}

bool LeasableLockOnNoSQLDatabase::ShouldRefreshLease() const noexcept {
  unique_lock<mutex> lock(mutex_);
  // Lease will be refreshed if
  // 1. Current Lease is expired
  // 2. Current Lease is not expired, the lease is owned by this caller and
  // lease renew threshold has been reached.
  // 3. Current Lease is not expired, the lease is NOT owned by this caller but
  // has passed atleast half of the lease duration since it was last refreshed.
  // (This is done to ensure we don't wait until the Lease becomes fully stale
  // and then refresh which leads to incorrect stats w.r.t. currently holding
  // lease count)
  if (!current_lease_.has_value()) {
    return true;
  } else if (current_lease_->IsExpired()) {
    return true;
  } else if (current_lease_->IsLeaseOwner(
                 lease_acquirer_info_.lease_acquirer_id)) {
    return current_lease_->IsLeaseRenewalRequired(
        lease_duration_in_milliseconds_,
        lease_renewal_threshold_percent_time_left_in_lease_);
  } else {
    return current_lease_->IsHalfLeaseDurationPassed(
        lease_duration_in_milliseconds_);
  }
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
  auto now_timestamp_in_milliseconds = GetCurrentTimeInMilliseconds();
  return now_timestamp_in_milliseconds >
         lease_expiraton_timestamp_in_milliseconds;
}

bool LeasableLockOnNoSQLDatabase::LeaseInfoInternal::IsLeaseOwner(
    string lock_acquirer_id) const {
  return lease_owner_info.lease_acquirer_id == lock_acquirer_id;
}

void LeasableLockOnNoSQLDatabase::LeaseInfoInternal::
    SetExpirationTimestampFromNow(milliseconds lease_duration_in_milliseconds) {
  lease_expiraton_timestamp_in_milliseconds =
      GetCurrentTimeInMilliseconds() + lease_duration_in_milliseconds;
}

bool LeasableLockOnNoSQLDatabase::LeaseInfoInternal::IsLeaseRenewalRequired(
    milliseconds lease_duration_in_milliseconds,
    uint64_t lease_renewal_threshold_percent_time_left_in_lease) const {
  auto now_timestamp_in_milliseconds = GetCurrentTimeInMilliseconds();
  if (now_timestamp_in_milliseconds >
      lease_expiraton_timestamp_in_milliseconds) {
    return true;
  }
  auto time_left_in_lease_in_milliseconds =
      lease_expiraton_timestamp_in_milliseconds - now_timestamp_in_milliseconds;
  double percent_time_left =
      (time_left_in_lease_in_milliseconds.count() * 100) /
      lease_duration_in_milliseconds.count();
  return (percent_time_left <
          lease_renewal_threshold_percent_time_left_in_lease);
}

bool LeasableLockOnNoSQLDatabase::LeaseInfoInternal::IsHalfLeaseDurationPassed(
    milliseconds lease_duration_in_milliseconds) const {
  auto now_timestamp_in_milliseconds = GetCurrentTimeInMilliseconds();
  if (now_timestamp_in_milliseconds >
      lease_expiraton_timestamp_in_milliseconds) {
    return true;
  }
  auto time_left_in_lease_in_milliseconds =
      lease_expiraton_timestamp_in_milliseconds - now_timestamp_in_milliseconds;
  return (time_left_in_lease_in_milliseconds.count() <
          (lease_duration_in_milliseconds.count() / 2.0));
}

milliseconds
LeasableLockOnNoSQLDatabase::LeaseInfoInternal::GetCurrentTimeInMilliseconds()
    const {
  return duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds());
}
};  // namespace google::scp::pbs
