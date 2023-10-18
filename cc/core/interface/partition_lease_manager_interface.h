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

#include <vector>

#include "core/interface/partition_interface.h"
#include "core/interface/partition_types.h"
#include "core/interface/service_interface.h"
#include "core/interface/type_def.h"

namespace google::scp::core {

/**
 * @brief PartitionLeaseManagerInterface
 * Acquire and Renew leases on partitions. This uses LeaseManager internally to
 * manage leases.
 */
class PartitionLeaseManagerInterface : public ServiceInterface {
 public:
  ~PartitionLeaseManagerInterface() = default;
  /**
   * @brief Instruct lease manager to start managing (acquire and renew) leases
   * on any partitions of count specified. This method can be invoked multiple
   * times to increase/decrease the count.
   *
   * @param partition_count
   * @return ExecutionResult
   */
  virtual ExecutionResult LeasePartitions(size_t partition_count) noexcept = 0;

  /**
   * @brief Instruct lease manager to start managing leases on specific
   * partitions as specified. This method can be invoked multiple times to
   * update the set of partitions.
   *
   * @param partitions
   * @return ExecutionResult
   */
  virtual ExecutionResult LeasePartitions(
      const std::vector<PartitionId>& partitions) noexcept = 0;
};

}  // namespace google::scp::core
