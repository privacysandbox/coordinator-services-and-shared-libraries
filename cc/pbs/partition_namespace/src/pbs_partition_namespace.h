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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/partition_namespace_interface.h"

namespace google::scp::pbs {
/**
 * @copydoc PartitionNamespaceInterface
 *
 * PartitionNamespace is statically configured with a fixed set of
 * partitions that does not change for the lifetime of the deployment.
 */
class PBSPartitionNamespace : public core::PartitionNamespaceInterface {
 public:
  /**
   * @brief Partitions are obtained statically from an external source such as a
   * deployment configuration file.
   *
   * @param partitions
   */
  explicit PBSPartitionNamespace(
      const std::vector<core::PartitionId>& partitions)
      : partitions_(partitions) {
    assert(partitions_.size() > 0);
  }

  core::PartitionId MapResourceToPartition(
      const core::ResourceId&) noexcept override;

  const std::vector<core::PartitionId>& GetPartitions()
      const noexcept override {
    return partitions_;
  }

 protected:
  const std::vector<core::PartitionId> partitions_;
  std::hash<core::ResourceId> resource_hasher_;
};
}  // namespace google::scp::pbs
