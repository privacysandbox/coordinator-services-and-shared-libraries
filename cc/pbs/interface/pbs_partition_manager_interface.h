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

#include "core/interface/partition_manager_interface.h"
#include "pbs/interface/pbs_partition_interface.h"

namespace google::scp::pbs {
/**
 * @brief PBS Partition Manager interface.
 */
class PBSPartitionManagerInterface : public core::PartitionManagerInterface {
 public:
  /**
   * @brief Get the Partition object for the Partition ID if already loaded.
   *
   * @param partitionId
   * @return ExecutionResultOr<std::shared_ptr<Partition>>
   */
  virtual core::ExecutionResultOr<std::shared_ptr<PBSPartitionInterface>>
  GetPBSPartition(const core::PartitionId& partition_id) noexcept = 0;
};
}  // namespace google::scp::pbs
