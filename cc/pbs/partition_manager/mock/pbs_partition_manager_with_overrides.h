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

#pragma once
#include <memory>

#include "pbs/partition_manager/src/pbs_partition_manager.h"

namespace google::scp::pbs::partition_manager::mock {

class PBSPartitionManagerWithOverrides : public PBSPartitionManager {
 public:
  PBSPartitionManagerWithOverrides(
      PBSPartition::Dependencies partition_dependencies,
      size_t partition_transaction_manager_capacity)
      : PBSPartitionManager(partition_dependencies,
                            partition_transaction_manager_capacity) {}

  std::shared_ptr<PBSPartitionInterface> ConstructPBSPartition(
      const core::PartitionId& partition_id,
      const core::PartitionType& partition_type) noexcept override {
    if (construct_partition_override_) {
      return construct_partition_override_(partition_id, partition_type);
    }
    return PBSPartitionManager::ConstructPBSPartition(partition_id,
                                                      partition_type);
  }

  void SetConfigProvider(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider) {
    partition_dependencies_.config_provider = config_provider;
  }

  std::function<std::shared_ptr<PBSPartitionInterface>(
      const core::PartitionId&, const core::PartitionType&)>
      construct_partition_override_;
};

}  // namespace google::scp::pbs::partition_manager::mock
