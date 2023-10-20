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
#include <shared_mutex>
#include <string>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/interface/partition_manager_interface.h"
#include "pbs/partition/src/pbs_partition.h"

namespace google::scp::pbs {
struct PBSPartitionManagerMapEntry {
  PBSPartitionManagerMapEntry(
      const core::PartitionMetadata& metadata,
      const std::shared_ptr<PBSPartitionInterface>& partition)
      : partition_id(metadata.partition_id),
        partition_type(metadata.partition_type),
        partition_address_uri_(std::make_shared<core::PartitionAddressUri>(
            metadata.partition_address_uri)),
        partition_handle(partition) {}

  void SetPartitionAddress(
      const core::PartitionAddressUri& partition_address_uri) {
    std::unique_lock lock(address_mutex_);
    partition_address_uri_ =
        std::make_shared<core::PartitionAddressUri>(partition_address_uri);
  }

  std::shared_ptr<core::PartitionAddressUri> GetPartitionAddress() {
    std::shared_lock lock(address_mutex_);
    return partition_address_uri_;
  }

  const core::PartitionId partition_id;
  const core::PartitionType partition_type;
  const std::shared_ptr<PBSPartitionInterface> partition_handle;

 protected:
  std::shared_ptr<core::PartitionAddressUri> partition_address_uri_;
  std::shared_mutex address_mutex_;
};
}  // namespace google::scp::pbs
