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

#include "pbs/partition_lease_preference_applier/src/partition_lease_preference_applier.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/lease_manager/mock/mock_lease_acquistion_preference.h"
#include "core/lease_manager/mock/mock_lease_statistics.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::LeaseAcquisitionPreference;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::lease_manager::mock::MockLeaseAcquisitionPreference;
using google::scp::core::lease_manager::mock::MockLeaseStatistics;
using std::make_shared;
using testing::Return;

namespace google::scp::pbs::test {
TEST(PartitionLeasePreferenceApplierTest, NinePartitionsTwoVirtualNodes) {
  auto lease_stats = make_shared<MockLeaseStatistics>();
  auto lease_acquisition_preference =
      make_shared<MockLeaseAcquisitionPreference>();
  auto partition_count = 9;
  auto num_nodes = 2;

  EXPECT_CALL(*lease_stats, GetCurrentlyLeasedLocksCount)
      .WillOnce(Return(num_nodes));
  EXPECT_CALL(*lease_acquisition_preference,
              SetLeaseAcquisitionPreference(LeaseAcquisitionPreference{5, {}}))
      .WillOnce(Return(SuccessExecutionResult()));

  PartitionLeasePreferenceApplier equal_partition_hosting_manager(
      partition_count, lease_stats, lease_acquisition_preference);
  equal_partition_hosting_manager.ApplyLeasePreference();
}

TEST(PartitionLeasePreferenceApplierTest, NinePartitionsOneVirtualNode) {
  auto lease_stats = make_shared<MockLeaseStatistics>();
  auto lease_acquisition_preference =
      make_shared<MockLeaseAcquisitionPreference>();
  auto partition_count = 9;
  auto num_nodes = 1;

  EXPECT_CALL(*lease_stats, GetCurrentlyLeasedLocksCount)
      .WillOnce(Return(num_nodes));
  EXPECT_CALL(*lease_acquisition_preference,
              SetLeaseAcquisitionPreference(LeaseAcquisitionPreference{9, {}}))
      .WillOnce(Return(SuccessExecutionResult()));

  PartitionLeasePreferenceApplier equal_partition_hosting_manager(
      partition_count, lease_stats, lease_acquisition_preference);
  equal_partition_hosting_manager.ApplyLeasePreference();
}

TEST(PartitionLeasePreferenceApplierTest, NinePartitionsThreeVirtualNodes) {
  auto lease_stats = make_shared<MockLeaseStatistics>();
  auto lease_acquisition_preference =
      make_shared<MockLeaseAcquisitionPreference>();
  auto partition_count = 9;
  auto num_nodes = 3;

  EXPECT_CALL(*lease_stats, GetCurrentlyLeasedLocksCount)
      .WillOnce(Return(num_nodes));
  EXPECT_CALL(*lease_acquisition_preference,
              SetLeaseAcquisitionPreference(LeaseAcquisitionPreference{3, {}}))
      .WillOnce(Return(SuccessExecutionResult()));

  PartitionLeasePreferenceApplier equal_partition_hosting_manager(
      partition_count, lease_stats, lease_acquisition_preference);
  equal_partition_hosting_manager.ApplyLeasePreference();
}
}  // namespace google::scp::pbs::test
