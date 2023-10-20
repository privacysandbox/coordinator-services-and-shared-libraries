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

#include "pbs/interface/pbs_partition_manager_interface.h"

namespace google::scp::pbs::partition_manager::mock {
class MockPBSPartitionManager : public PBSPartitionManagerInterface {
 public:
  MOCK_METHOD(core::ExecutionResult, Init, (), (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, Run, (), (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, Stop, (), (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, LoadPartition,
              (const core::PartitionMetadata& partitionInfo),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, UnloadPartition,
              (const core::PartitionMetadata& partitionInfo),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, RefreshPartitionAddress,
              (const core::PartitionMetadata& partition_address),
              (noexcept, override));

  MOCK_METHOD(
      (core::ExecutionResultOr<std::shared_ptr<core::PartitionAddressUri>>),
      GetPartitionAddress, (const core::PartitionId& partition_id),
      (noexcept, override));

  MOCK_METHOD((core::ExecutionResultOr<core::PartitionType>), GetPartitionType,
              (const core::PartitionId& partition_id), (noexcept, override));

  MOCK_METHOD(
      (core::ExecutionResultOr<std::shared_ptr<core::PartitionInterface>>),
      GetPartition, (const core::PartitionId& partition_id),
      (noexcept, override));

  MOCK_METHOD((core::ExecutionResultOr<std::shared_ptr<PBSPartitionInterface>>),
              GetPBSPartition, (const core::PartitionId& partition_id),
              (noexcept, override));
};
}  // namespace google::scp::pbs::partition_manager::mock
