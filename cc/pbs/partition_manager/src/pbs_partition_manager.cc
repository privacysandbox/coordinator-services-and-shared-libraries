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

#include "pbs/partition_manager/src/pbs_partition_manager.h"

#include <utility>
#include <vector>

#include "pbs/interface/configuration_keys.h"
#include "pbs/partition/src/error_codes.h"
#include "pbs/partition/src/pbs_partition.h"
#include "pbs/partition/src/remote_pbs_partition.h"
#include "pbs/partition_manager/src/error_codes.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::PartitionAddressUri;
using google::scp::core::PartitionId;
using google::scp::core::PartitionInterface;
using google::scp::core::PartitionMetadata;
using google::scp::core::PartitionType;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::ToString;
using google::scp::pbs::RemotePBSPartition;
using std::make_pair;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;

static constexpr char kPBSPartitionManager[] = "PBSPartitionManager";

namespace google::scp::pbs {

PBSPartitionManager::PBSPartitionManager(
    PBSPartition::Dependencies partition_dependencies,
    size_t partition_transaction_manager_capacity)
    : partition_dependencies_(move(partition_dependencies)),
      partition_transaction_manager_capacity_(
          partition_transaction_manager_capacity),
      is_running_(false) {}

ExecutionResult PBSPartitionManager::Init() noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_ALREADY_RUNNING);
  }

  // Read the bucket name where the partition's journals will be stored.
  partition_journal_bucket_name_ = make_shared<string>();
  auto execution_result = partition_dependencies_.config_provider->Get(
      kJournalServiceBucketName, *partition_journal_bucket_name_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kPBSPartitionManager, kZeroUuid, execution_result,
              "Failed to read journal bucket name.");
    return execution_result;
  }

  SCP_INFO(kPBSPartitionManager, kZeroUuid, "Journal Bucket Name: '%s'",
           partition_journal_bucket_name_->c_str());

  return SuccessExecutionResult();
}

ExecutionResult PBSPartitionManager::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_ALREADY_RUNNING);
  }
  is_running_ = true;
  return SuccessExecutionResult();
}

ExecutionResult PBSPartitionManager::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  is_running_ = false;

  // Unload all of partitions
  vector<core::common::Uuid> loaded_partition_ids;
  RETURN_IF_FAILURE(loaded_partitions_map_.Keys(loaded_partition_ids));

  // Wait until all partitions converge to Unloaded state
  while (loaded_partition_ids.size() > 0) {
    for (const auto& loaded_partition_id : loaded_partition_ids) {
      shared_ptr<PBSPartitionManagerMapEntry> loaded_partition_entry;
      if (!loaded_partitions_map_
               .Find(loaded_partition_id, loaded_partition_entry)
               .Successful()) {
        continue;
      }
      // Perform unload only if is loaded.
      if (loaded_partition_entry->partition_handle->GetPartitionState() ==
          core::PartitionLoadUnloadState::Loaded) {
        auto execution_result =
            loaded_partition_entry->partition_handle->Unload();
        // If cannot be unloaded due to concurrent unload by another thread,
        // do nothing.
        if (!execution_result.Successful() &&
            (execution_result !=
             FailureExecutionResult(
                 core::errors::SC_PBS_PARTITION_CANNOT_UNLOAD))) {
          return execution_result;
        }
      }
      if (loaded_partition_entry->partition_handle->GetPartitionState() ==
          core::PartitionLoadUnloadState::Unloaded) {
        loaded_partitions_map_.Erase(loaded_partition_id);
      }
    }
    // Refresh the partitions list
    RETURN_IF_FAILURE(loaded_partitions_map_.Keys(loaded_partition_ids));
  }

  return SuccessExecutionResult();
}

shared_ptr<PBSPartitionInterface> PBSPartitionManager::ConstructPBSPartition(
    const PartitionId& partition_id,
    const PartitionType& partition_type) noexcept {
  if (partition_type == PartitionType::Remote) {
    return make_shared<RemotePBSPartition>();
  }
  return make_shared<PBSPartition>(partition_id, partition_dependencies_,
                                   partition_journal_bucket_name_,
                                   partition_transaction_manager_capacity_);
}

ExecutionResult PBSPartitionManager::LoadPartition(
    const PartitionMetadata& partition_metadata) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  shared_ptr<PBSPartitionInterface> partition = ConstructPBSPartition(
      partition_metadata.partition_id, partition_metadata.partition_type);
  auto partition_map_entry =
      make_shared<PBSPartitionManagerMapEntry>(partition_metadata, partition);
  auto partition_map_pair =
      make_pair(partition_metadata.Id(), partition_map_entry);
  RETURN_IF_FAILURE(
      loaded_partitions_map_.Insert(partition_map_pair, partition_map_entry));

  // If the partition manager is unloading while this thread is inserting, we
  // must ensure that the partition is discarded since the unloading thread
  // might miss erasing this due to the race.
  if (!is_running_) {
    loaded_partitions_map_.Erase(partition_metadata.partition_id);
    return SuccessExecutionResult();
  }

  auto execution_result = partition->Init();
  if (!execution_result.Successful()) {
    loaded_partitions_map_.Erase(partition_metadata.partition_id);
    SCP_ERROR(kPBSPartitionManager, partition_metadata.partition_id,
              execution_result, "Cannot Load partition");
    return FailureExecutionResult(core::errors::SC_PBS_PARTITION_LOAD_FAILURE);
  }

  execution_result = partition->Load();
  if (!execution_result.Successful()) {
    loaded_partitions_map_.Erase(partition_metadata.partition_id);
    SCP_ERROR(kPBSPartitionManager, partition_metadata.partition_id,
              execution_result, "Cannot Load partition");
    return FailureExecutionResult(core::errors::SC_PBS_PARTITION_LOAD_FAILURE);
  }

  return SuccessExecutionResult();
}

ExecutionResult PBSPartitionManager::RefreshPartitionAddress(
    const PartitionMetadata& partition_metadata) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  shared_ptr<PBSPartitionManagerMapEntry> partition_map_entry;
  RETURN_IF_FAILURE(loaded_partitions_map_.Find(partition_metadata.partition_id,
                                                partition_map_entry));
  if (partition_map_entry->partition_type !=
      partition_metadata.partition_type) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_INVALID_REQUEST);
  }

  partition_map_entry->SetPartitionAddress(
      partition_metadata.partition_address_uri);

  return SuccessExecutionResult();
}

ExecutionResultOr<shared_ptr<PartitionAddressUri>>
PBSPartitionManager::GetPartitionAddress(
    const PartitionId& partition_id) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  shared_ptr<PBSPartitionManagerMapEntry> partition_map_entry;
  RETURN_IF_FAILURE(
      loaded_partitions_map_.Find(partition_id, partition_map_entry));

  return partition_map_entry->GetPartitionAddress();
}

ExecutionResultOr<PartitionType> PBSPartitionManager::GetPartitionType(
    const core::PartitionId& partition_id) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  shared_ptr<PBSPartitionManagerMapEntry> partition_map_entry;
  RETURN_IF_FAILURE(
      loaded_partitions_map_.Find(partition_id, partition_map_entry));

  return partition_map_entry->partition_type;
}

ExecutionResult PBSPartitionManager::UnloadPartition(
    const PartitionMetadata& partition_metadata) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  auto partition_id = partition_metadata.Id();
  shared_ptr<PBSPartitionManagerMapEntry> partition_map_entry;
  RETURN_IF_FAILURE(
      loaded_partitions_map_.Find(partition_id, partition_map_entry));

  auto execution_result = partition_map_entry->partition_handle->Unload();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPBSPartitionManager, partition_id, execution_result,
              "Cannot Unload partition");
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_UNLOAD_FAILURE);
  }

  // Erase the entry only if it could be completely unloaded. This avoids
  // potential load on the same partition to happen concurrently while the
  // unloading is happening.
  RETURN_IF_FAILURE(loaded_partitions_map_.Erase(partition_id));

  return SuccessExecutionResult();
}

ExecutionResultOr<shared_ptr<PartitionInterface>>
PBSPartitionManager::GetPartition(const PartitionId& partition_id) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  shared_ptr<PBSPartitionManagerMapEntry> partition_map_entry;
  RETURN_IF_FAILURE(
      loaded_partitions_map_.Find(partition_id, partition_map_entry));

  return partition_map_entry->partition_handle;
}

ExecutionResultOr<shared_ptr<PBSPartitionInterface>>
PBSPartitionManager::GetPBSPartition(const PartitionId& partition_id) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING);
  }

  shared_ptr<PBSPartitionManagerMapEntry> partition_map_entry;
  RETURN_IF_FAILURE(
      loaded_partitions_map_.Find(partition_id, partition_map_entry));

  return partition_map_entry->partition_handle;
}

}  // namespace google::scp::pbs
