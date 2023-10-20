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

#include "core/common/uuid/src/uuid.h"
#include "core/interface/initializable_interface.h"
#include "core/interface/partition_interface.h"

namespace google::scp::core {

/**
 * @brief Represents types of Partitions that can be loaded.
 *
 * Local: The Partition's home address is this instance.
 * Remote: The Partition's home address is another instance.
 *
 * NOTE: If lease is obtained on a partition by this instance, then it is
 * considered to be home on this instance.
 */
enum class PartitionType { Local, Remote };

/**
 * @brief Represents the current state of partition w.r.t load/unload.
 *
 */
enum class PartitionLoadUnloadState : uint64_t {
  /// @brief After Partition object is created. This is the initial
  /// state after object creation.
  Created,
  /// @brief After Init() is completed successfully.
  Initialized,
  /// @brief State during Load()ing of the partition.
  Loading,
  /// @brief After Loading the partition successfully. This is the steady state
  /// of the partition. Incoming transaction requests can be accepted only at
  /// this state.
  Loaded,
  /// @brief State during Unload()ing of the partition.
  Unloading,
  /// @brief After unloading the partition successfully. Partition object to be
  /// destroyed and cannot be reused. This is a terminal state.
  Unloaded
};

/**
 * @brief Partition represents encapsulation of components that operate on a
 * partitioned space of data/namespace.
 */
class PartitionInterface : public InitializableInterface {
 public:
  ~PartitionInterface() = default;
  /**
   * @brief Initialize required data for loading the partition.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult Init() noexcept = 0;

  /**
   * @brief Load a partition from store and bring it into life.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult Load() noexcept = 0;

  /**
   * @brief Teardown partition by stopping partition's work and discarding any
   * pending work.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult Unload() noexcept = 0;

  /**
   * @brief Returns the current state of the partition w.r.t. load/unload
   *
   * @return PartitionLoadUnloadState
   */
  virtual PartitionLoadUnloadState GetPartitionState() noexcept = 0;
};

}  // namespace google::scp::core
