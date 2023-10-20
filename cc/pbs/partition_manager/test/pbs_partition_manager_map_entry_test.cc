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

#include "pbs/partition_manager/src/pbs_partition_manager_map_entry.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::PartitionMetadata;
using google::scp::core::PartitionType;
using google::scp::core::common::Uuid;
using google::scp::pbs::PBSPartitionManagerMapEntry;

namespace google::scp::pbs::test {

TEST(PartitionMapEntryTest, Initialization) {
  PartitionMetadata metadata(Uuid(), PartitionType::Remote,
                             "https://www.google.com");
  std::shared_ptr<PBSPartitionInterface> partition;
  PBSPartitionManagerMapEntry map_entry(metadata, partition);
  EXPECT_EQ(*map_entry.GetPartitionAddress(), "https://www.google.com");
  EXPECT_EQ(map_entry.partition_handle, partition);
  EXPECT_EQ(map_entry.partition_id, Uuid());
  EXPECT_EQ(map_entry.partition_type, PartitionType::Remote);
}

TEST(PartitionMapEntryTest, PartitionAddressCanBeReset) {
  PartitionMetadata metadata(Uuid(), PartitionType::Remote,
                             "https://www.google.com");
  std::shared_ptr<PBSPartitionInterface> partition;
  PBSPartitionManagerMapEntry map_entry(metadata, partition);
  EXPECT_EQ(*map_entry.GetPartitionAddress(), "https://www.google.com");
  map_entry.SetPartitionAddress("https://www.google.com:80");
  EXPECT_EQ(*map_entry.GetPartitionAddress(), "https://www.google.com:80");
}

TEST(PartitionMapEntryTest, PartitionAddressCanBeResetConcurrently) {
  PartitionMetadata metadata(Uuid(), PartitionType::Remote,
                             "https://www.google.com");
  std::shared_ptr<PBSPartitionInterface> partition;
  PBSPartitionManagerMapEntry map_entry(metadata, partition);
  EXPECT_EQ(*map_entry.GetPartitionAddress(), "https://www.google.com");

  std::atomic<bool> stop = false;
  std::thread getter_thread([&stop, &map_entry]() {
    while (!stop) {
      EXPECT_EQ(*map_entry.GetPartitionAddress(), "https://www.google.com");
    }
  });

  std::thread setter_thread([&stop, &map_entry]() {
    while (!stop) {
      map_entry.SetPartitionAddress("https://www.google.com");
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(1));
  stop = true;
  getter_thread.join();
  setter_thread.join();
}

}  // namespace google::scp::pbs::test
