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
#include <thread>

#include "core/interface/lease_manager_interface.h"

namespace google::scp::pbs {
/**
 * @brief Lease Preference Applier to host equal number of partitions on all of
 * the virtual nodes.
 */
class PartitionLeasePreferenceApplier : public core::ServiceInterface {
 public:
  PartitionLeasePreferenceApplier(
      size_t partition_count,
      std::shared_ptr<core::LeaseStatisticsInterface>
          virtual_node_lease_statistics,
      std::shared_ptr<core::LeaseAcquisitionPreferenceInterface>
          partition_lease_acquisition_preference);

  /**
   * @brief Apply partition lease preference with help of information from the
   * virtual node lease statistics.
   *
   * @return core::ExecutionResult
   */
  core::ExecutionResult ApplyLeasePreference() noexcept;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  void ThreadFunction();

 protected:
  /// @brief Count of partitions in the system
  const size_t partition_count_;
  /// @brief Virtual node lease statistics
  std::shared_ptr<core::LeaseStatisticsInterface> virtual_node_lease_stats_;
  /// @brief Partition lease acquisition preference handle
  std::shared_ptr<core::LeaseAcquisitionPreferenceInterface>
      partition_lease_acquisition_preference_;
  /// @brief thread that runs the lease preference application
  std::unique_ptr<std::thread> worker_thread_;
  /// @brief is worker_thread_ running.
  std::atomic<bool> is_running_;
  /// @brief mutex to serialize work
  std::mutex work_mutex_;
  /// @brief activity ID of the run
  core::common::Uuid object_activity_id_;
};

}  // namespace google::scp::pbs
