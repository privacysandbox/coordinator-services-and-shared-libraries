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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/interface/lease_manager_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "pbs/interface/configuration_keys.h"

namespace google::scp::pbs {

constexpr std::chrono::milliseconds kDefaultLeaseDurationInMilliseconds =
    std::chrono::seconds(10);
constexpr uint64_t kLeaseRenewalThresholdPercentTimeLeftInLease =
    50;  // Start renewing lease when the remaining lease duration <= 50% of
         // total lease duration.

/**
 * @copydoc core::LeasableLockInterface
 */
class LeasableLockOnNoSQLDatabase : public core::LeasableLockInterface {
 public:
  /**
   * @brief Construct a new Leasable Lock object
   *
   * @param database nosql database acessor object
   * @param lease_acquirer_info lease acquirer info of PBS lease acquirer
   * @param lock_row_key identifier of lock to hold lease on. Each PBS partition
   * get its own lock.
   * @param lease_duration_in_milliseconds time duration for which the lease
   * needs to be acquired or renewed.
   * @param lease_renewal_threshold_percent_time_left_in_lease_ percentage of
   * time left in lease at which lease renewal should start.
   */
  LeasableLockOnNoSQLDatabase(
      std::shared_ptr<core::NoSQLDatabaseProviderInterface> database,
      core::LeaseInfo lease_acquirer_info, std::string table_name,
      std::string lock_row_key = kPBSPartitionLockTableRowKeyForGlobalPartition,
      std::chrono::milliseconds lease_duration_in_milliseconds =
          kDefaultLeaseDurationInMilliseconds,
      uint64_t lease_renewal_threshold_percent_time_left_in_lease =
          kLeaseRenewalThresholdPercentTimeLeftInLease) noexcept;

  /**
   * @brief Refreshes lease on the lock present on no-sql database. If the lease
   * refresh fails, an error status code is returned.
   *
   * @return core::ExecutionResult
   */
  core::ExecutionResult RefreshLease(
      bool is_read_only_lease_refresh) noexcept override;

  /**
   * @brief Determines if lease refresh needs to be done based on cached lease
   * information. If there is no cached lease information, this returns true.
   *
   * @return true
   * @return false
   */
  bool ShouldRefreshLease() const noexcept override;

  /**
   * @brief Returns true if the current node owns lease on the lock.
   *
   * @return true
   * @return false
   */
  bool IsCurrentLeaseOwner() const noexcept override;

  /**
   * @brief Get the Current PBS Lease Owner Info object if the cached lease
   * information is valid. If cached information is invalid, returns {}
   *
   * @return std::optional<core::LeaseInfo>
   */
  std::optional<core::LeaseInfo> GetCurrentLeaseOwnerInfo()
      const noexcept override;

  core::TimeDuration GetConfiguredLeaseDurationInMilliseconds()
      const noexcept override;

 protected:
  struct LeaseInfoInternal {
    LeaseInfoInternal(
        const core::LeaseInfo& lease_owner_info,
        std::chrono::milliseconds lease_expiraton_timestamp_in_milliseconds =
            std::chrono::milliseconds(0))
        : lease_owner_info(lease_owner_info),
          lease_expiraton_timestamp_in_milliseconds(
              lease_expiraton_timestamp_in_milliseconds),
          lease_acquisition_disallowed(false) {}

    LeaseInfoInternal()
        : lease_owner_info(),
          lease_expiraton_timestamp_in_milliseconds(0),
          lease_acquisition_disallowed(false) {}

    bool IsExpired() const;

    /// @brief Set the expiration timestamp to be now() + lease_duration
    /// @param lease_duration
    void SetExpirationTimestampFromNow(
        std::chrono::milliseconds lease_duration);

    bool IsLeaseOwner(std::string lease_acquirer_id) const;

    /// @brief Should renew? is calculated as percent time left in the
    /// current lease according to
    /// 'lease_renewal_threshold_percent_time_left_in_lease'
    /// @param lease_duration_in_milliseconds
    /// @param lease_renewal_threshold_percent_time_left_in_lease
    /// @return
    bool IsLeaseRenewalRequired(
        std::chrono::milliseconds lease_duration_in_milliseconds,
        uint64_t lease_renewal_threshold_percent_time_left_in_lease) const;

    std::chrono::milliseconds GetCurrentTimeInMilliseconds() const;

    /// @brief Is < half of the lease time remaining in the current lease.
    /// @param lease_duration_in_milliseconds
    /// @return
    bool IsHalfLeaseDurationPassed(
        std::chrono::milliseconds lease_duration_in_milliseconds) const;

    /// @brief Lease owner information.
    core::LeaseInfo lease_owner_info;
    /// @brief The expiration timestamp in milliseconds
    std::chrono::milliseconds lease_expiraton_timestamp_in_milliseconds;
    /// @brief Indicates if the lease acquisition needs to be disallowed.
    /// NOTE: This gets populated from the Database row into memory.
    bool lease_acquisition_disallowed;
  };

  // Database helper functions
  core::ExecutionResult WriteLeaseSynchronouslyToDatabase(
      const LeaseInfoInternal& previous_lease,
      const LeaseInfoInternal& new_lease);
  core::ExecutionResult ReadLeaseSynchronouslyFromDatabase(
      LeaseInfoInternal& lease);
  core::ExecutionResult ConstructAttributesFromLeaseInfo(
      const LeaseInfoInternal& lease,
      std::shared_ptr<std::vector<core::NoSqlDatabaseKeyValuePair>>&
          attributes);
  core::ExecutionResult ObtainLeaseInfoFromAttributes(
      const std::shared_ptr<std::vector<core::NoSqlDatabaseKeyValuePair>>&
          attributes,
      LeaseInfoInternal& lease);

  /**
   * @brief NoSQL database accessor
   *
   */
  std::shared_ptr<core::NoSQLDatabaseProviderInterface> database_;

  /**
   * @brief Current cached lease representing the lease value present on the
   * NoSQL database lock row.
   */
  std::optional<LeaseInfoInternal> current_lease_;

  /**
   * @brief object guard mutex
   */
  mutable std::mutex mutex_;

  /**
   * @brief Identity of the lease acquirer
   */
  const core::LeaseInfo lease_acquirer_info_;

  /**
   * @brief Table name on the NoSQL database
   */
  const std::string table_name_;

  /**
   * @brief Row key (lock ID) of the lock on the NoSQL database.
   */
  const std::string lock_row_key_;

  /**
   * @brief Duration of the lease that is configured on the lock.
   */
  const std::chrono::milliseconds lease_duration_in_milliseconds_;

  /**
   * @brief Threshold in percentage of time left in the lease at which the lease
   * should be allowed to renew.
   */
  const uint64_t lease_renewal_threshold_percent_time_left_in_lease_;

  /**
   * @brief Activity ID of the object
   *
   */
  const core::common::Uuid activity_id_;
};
};  // namespace google::scp::pbs
