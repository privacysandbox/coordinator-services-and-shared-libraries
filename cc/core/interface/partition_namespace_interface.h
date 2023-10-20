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
#include <vector>

#include "core/interface/partition_types.h"

namespace google::scp::core {
/**
 * @brief An abstraction to represent partitioned namespace that allows
 * resources to be mapped onto a partitioned ResourceId namespace.
 *
 * Given a resource, this module allows a namespace partitioning
 * and mapping scheme for the resource.
 *
 * Implementation of this interface could either define a static partitioning
 * scheme where the number of partitions do not change for the lifetime of the
 * system, or, the scheme could be a dynamic one, which involves resource
 * namespace range splitting/merging based on system's quality metrics.
 *
 */
class PartitionNamespaceInterface {
 public:
  virtual ~PartitionNamespaceInterface() = default;

  /**
   * @brief Maps a resource identifier to a partition in the resource identifier
   * namespace according to implementation defined partitioning scheme.
   *
   * @return PartitionId
   */
  virtual PartitionId MapResourceToPartition(const ResourceId&) noexcept = 0;

  /**
   * @brief Returns a list of all the partitions that can be mapped to in the
   * namespace.
   *
   * @return std::vector<PartitionId>
   */
  virtual const std::vector<PartitionId>& GetPartitions() const noexcept = 0;
};
}  // namespace google::scp::core
