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

#include "pbs/partition_namespace/src/pbs_partition_namespace.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/partition_types.h"

using google::scp::core::PartitionId;
using google::scp::core::ResourceId;

namespace google::scp::pbs::test {

TEST(PartitionNamespace, MapResourceIdsMapsEmptyResourceToSinglePartition) {
  std::vector<PartitionId> partitions = {PartitionId{1, 1}};
  PBSPartitionNamespace partition_namespace(partitions);
  EXPECT_EQ(partition_namespace.MapResourceToPartition(""),
            partition_namespace.MapResourceToPartition(""));
}

TEST(PartitionNamespace, MapResourceIdsMapsAllToSinglePartition) {
  std::vector<PartitionId> partitions = {PartitionId{1, 1}};
  PBSPartitionNamespace partition_namespace(partitions);
  EXPECT_EQ(partition_namespace.MapResourceToPartition("google.com"),
            partition_namespace.MapResourceToPartition("goog.com"));
}

// This test is used for creating resource IDs and their associated partition
// IDs. Enable it as necessary.
TEST(PartitionNamespace, PrintSomeResourceIdsForPartitions) {
  std::vector<PartitionId> partitions = {
      PartitionId{0, 1}, PartitionId{0, 2}, PartitionId{0, 3},
      PartitionId{0, 4}, PartitionId{0, 5}, PartitionId{0, 6},
      PartitionId{0, 7}, PartitionId{0, 8}, PartitionId{0, 9}};
  PBSPartitionNamespace partition_namespace(partitions);
  for (int i = 0; i < 100; i++) {
    auto resource = ("google" + std::to_string(i)) + ".com";
    std::cout << resource << " "
              << ToString(partition_namespace.MapResourceToPartition(resource))
              << std::endl;
  }
}

TEST(PartitionNamespace, MapResourceIdsMapsSinglePartitionMultipleResources) {
  std::vector<PartitionId> partitions = {PartitionId{1, 1}};
  PBSPartitionNamespace partition_namespace(partitions);
  size_t resource_count = 10000;
  PartitionId prev_mapped_partition = partitions[0];
  for (int i = 0; i < resource_count; i++) {
    ResourceId resource = std::to_string(i);
    auto mapped_partition =
        partition_namespace.MapResourceToPartition(resource);
    EXPECT_EQ(mapped_partition, prev_mapped_partition);
    prev_mapped_partition = mapped_partition;
  }
}

TEST(PartitionNamespace, MapResourceIdsMapsToDifferentPartitions) {
  std::vector<PartitionId> partitions = {PartitionId{1, 1}, PartitionId{1, 2}};
  PBSPartitionNamespace partition_namespace(partitions);
  EXPECT_NE(partition_namespace.MapResourceToPartition("google.com"),
            partition_namespace.MapResourceToPartition("goog.com"));
}

TEST(PartitionNamespace, MapResourceIdsMapsMultiplePartitionMultipleResources) {
  std::vector<PartitionId> partitions = {PartitionId{1, 1}, PartitionId{1, 2},
                                         PartitionId{1, 3}, PartitionId{1, 4},
                                         PartitionId{1, 5}};
  std::vector<size_t> partition_mapped_counts = {0, 0, 0, 0, 0};
  PBSPartitionNamespace partition_namespace(partitions);

  size_t resource_count = 10000;
  std::srand(std::chrono::steady_clock::now().time_since_epoch().count());

  for (int i = 0; i < resource_count; i++) {
    ResourceId resource =
        std::to_string(i) + std::to_string(std::rand() % 10000);
    auto mapped_partition =
        partition_namespace.MapResourceToPartition(resource);
    for (int i = 0; i < partitions.size(); i++) {
      if (mapped_partition == partitions[i]) {
        partition_mapped_counts[i]++;
      }
    }
  }

  int total_resources_mapped = 0;
  for (int i = 0; i < partitions.size(); i++) {
    std::cout << "Partition " << ToString(partitions[i]) << " got "
              << partition_mapped_counts[i] << " resources" << std::endl;
    total_resources_mapped += partition_mapped_counts[i];
  }

  EXPECT_EQ(total_resources_mapped, resource_count);
}

}  // namespace google::scp::pbs::test
