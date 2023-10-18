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
#include <string>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "pbs/interface/pbs_partition_manager_interface.h"
#include "pbs/partition/src/pbs_partition.h"
#include "pbs/partition_manager/src/pbs_partition_manager_map_entry.h"

namespace google::scp::pbs {

/**
 * @copydoc PBSPartitionManagerInterface
 */
class PBSPartitionManager : public PBSPartitionManagerInterface {
 public:
  PBSPartitionManager(PBSPartition::Dependencies partition_dependencies,
                      size_t partition_transaction_manager_capacity);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult LoadPartition(
      const core::PartitionMetadata& partitionInfo) noexcept override;

  core::ExecutionResult UnloadPartition(
      const core::PartitionMetadata& partitionInfo) noexcept override;

  core::ExecutionResult RefreshPartitionAddress(
      const core::PartitionMetadata& partition_address) noexcept override;

  core::ExecutionResultOr<std::shared_ptr<core::PartitionAddressUri>>
  GetPartitionAddress(const core::PartitionId& partition_id) noexcept override;

  core::ExecutionResultOr<core::PartitionType> GetPartitionType(
      const core::PartitionId& partition_id) noexcept override;
  core::ExecutionResultOr<std::shared_ptr<core::PartitionInterface>>
  GetPartition(const core::PartitionId& partition_id) noexcept override;

  core::ExecutionResultOr<std::shared_ptr<PBSPartitionInterface>>
  GetPBSPartition(const core::PartitionId& partition_id) noexcept override;

 protected:
  /**
   * @brief Internal factory method for PBS partition.
   *
   * @param partition_id
   * @return std::shared_ptr<PBSPartitionInterface>
   */
  virtual std::shared_ptr<PBSPartitionInterface> ConstructPBSPartition(
      const core::PartitionId& partition_id,
      const core::PartitionType& partition_type) noexcept;

  /// @brief Dependencies to boot up a partition.
  PBSPartition::Dependencies partition_dependencies_;

  /// @brief bucket at which partition's journal files are available or written
  std::shared_ptr<std::string> partition_journal_bucket_name_;

  /// @brief Maximum number of transactions a partition can handle.
  size_t partition_transaction_manager_capacity_;

  /// @brief Indicates if the component is running
  std::atomic<bool> is_running_;

  /// @brief Map of loaded partitions
  core::common::ConcurrentMap<core::PartitionId,
                              std::shared_ptr<PBSPartitionManagerMapEntry>,
                              core::common::UuidCompare>
      loaded_partitions_map_;
};
}  // namespace google::scp::pbs
