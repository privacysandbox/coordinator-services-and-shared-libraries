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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/common/concurrent_map/src/error_codes.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/partition/mock/pbs_partition_mock.h"
#include "pbs/partition/src/pbs_partition.h"
#include "pbs/partition_manager/mock/pbs_partition_manager_with_overrides.h"
#include "pbs/partition_manager/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::PartitionId;
using google::scp::core::PartitionMetadata;
using google::scp::core::PartitionType;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::test::ResultIs;
using google::scp::pbs::PBSPartition;
using google::scp::pbs::partition::mock::MockPBSPartition;
using google::scp::pbs::partition_manager::mock::
    PBSPartitionManagerWithOverrides;
using std::make_shared;
using std::shared_ptr;
using testing::Return;

namespace google::scp::pbs::test {
class PBSPartitionManagerTest : public testing::Test {
 protected:
  PBSPartitionManagerTest()
      : partition_manager_(partition_dependencies_,
                           1000 /* transaction capacity */) {
    mock_partition_1 = make_shared<MockPBSPartition>();
    mock_partition_2 = make_shared<MockPBSPartition>();
    partition_manager_.construct_partition_override_ =
        [&](const PartitionId& partition_id,
            const PartitionType& partition_type) {
          if (partition_id == mock_partition_1_id) {
            return mock_partition_1;
          } else if (partition_id == mock_partition_2_id) {
            return mock_partition_2;
          }
          throw;
        };
    auto mock_config_provider = make_shared<MockConfigProvider>();
    mock_config_provider->Set(kJournalServiceBucketName, "budget");
    partition_manager_.SetConfigProvider(mock_config_provider);
  }

  PartitionId mock_partition_1_id = {0, 1};
  PartitionId mock_partition_2_id = {0, 2};
  shared_ptr<MockPBSPartition> mock_partition_1;
  shared_ptr<MockPBSPartition> mock_partition_2;
  PBSPartition::Dependencies partition_dependencies_;
  PBSPartitionManagerWithOverrides partition_manager_;
};

TEST_F(PBSPartitionManagerTest, InitSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
}

TEST_F(PBSPartitionManagerTest, RunSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());
}

TEST_F(PBSPartitionManagerTest, DoubleRunFailure) {
  EXPECT_SUCCESS(partition_manager_.Run());
  EXPECT_THAT(partition_manager_.Run(),
              ResultIs(FailureExecutionResult(
                  core::errors::SC_PBS_PARTITION_MANAGER_ALREADY_RUNNING)));
}

TEST_F(PBSPartitionManagerTest, StopAfterRunSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());
  EXPECT_SUCCESS(partition_manager_.Stop());
}

TEST_F(PBSPartitionManagerTest, StopBeforeRunFailure) {
  EXPECT_THAT(partition_manager_.Stop(),
              ResultIs(FailureExecutionResult(
                  core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING)));
  EXPECT_SUCCESS(partition_manager_.Run());
}

TEST_F(PBSPartitionManagerTest, LoadUnloadRequestsNotAllowedBeforeRun) {
  EXPECT_SUCCESS(partition_manager_.Init());
  PartitionMetadata partition_metadata(Uuid(), PartitionType::Local, "");
  EXPECT_THAT(partition_manager_.LoadPartition(partition_metadata),
              ResultIs(FailureExecutionResult(
                  core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING)));
  EXPECT_THAT(partition_manager_.UnloadPartition(partition_metadata),
              ResultIs(FailureExecutionResult(
                  core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING)));
}

TEST_F(PBSPartitionManagerTest, LoadUnloadRequestsNotAllowedAfterStop) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());
  EXPECT_SUCCESS(partition_manager_.Stop());
  PartitionMetadata partition_metadata(Uuid(), PartitionType::Local, "");
  EXPECT_THAT(partition_manager_.LoadPartition(partition_metadata),
              ResultIs(FailureExecutionResult(
                  core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING)));
  EXPECT_THAT(partition_manager_.UnloadPartition(partition_metadata),
              ResultIs(FailureExecutionResult(
                  core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING)));
}

TEST_F(PBSPartitionManagerTest, LoadPartitionSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());
  EXPECT_CALL(*mock_partition_1, Init)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_partition_1, Load)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_partition_1, Unload).Times(0);
  PartitionMetadata partition_metadata(
      mock_partition_1_id, PartitionType::Local, "https://localhost");
  EXPECT_SUCCESS(partition_manager_.LoadPartition(partition_metadata));
}

TEST_F(PBSPartitionManagerTest, LoadRemotePartitionSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());
  EXPECT_CALL(*mock_partition_1, Init)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_partition_1, Load)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_partition_1, Unload).Times(0);
  PartitionMetadata partition_metadata(
      mock_partition_1_id, PartitionType::Remote, "https://1.1.1.1:9090");
  EXPECT_SUCCESS(partition_manager_.LoadPartition(partition_metadata));
}

static void SetupMocksForAllPartitionMethods(
    shared_ptr<MockPBSPartition> mock_partition) {
  // Partition 1
  EXPECT_CALL(*mock_partition, Init)
      .WillOnce([&mock_partition = *mock_partition]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Initialized;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition, Load)
      .WillOnce([&mock_partition = *mock_partition]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Loaded;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition, Unload)
      .WillOnce([&mock_partition = *mock_partition]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Unloaded;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition, GetPartitionState)
      .WillRepeatedly([&mock_partition = *mock_partition]() {
        return mock_partition.partition_state_.load();
      });
}

TEST_F(PBSPartitionManagerTest, StopUnloadsAllLoadedPartitions) {
  SetupMocksForAllPartitionMethods(mock_partition_1);
  SetupMocksForAllPartitionMethods(mock_partition_2);

  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_2_id, PartitionType::Local, "https://localhost"}));

  EXPECT_SUCCESS(partition_manager_.Stop());
}

TEST_F(PBSPartitionManagerTest, StopFailsIfCannotUnloadPartition) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_CALL(*mock_partition_2, Init)
      .WillOnce([&mock_partition = *mock_partition_2]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Initialized;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition_2, Load)
      .WillOnce([&mock_partition = *mock_partition_2]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Loaded;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition_2, Unload)
      .WillRepeatedly(Return(FailureExecutionResult(1234)));
  EXPECT_CALL(*mock_partition_2, GetPartitionState)
      .WillRepeatedly([&mock_partition = *mock_partition_2]() {
        return mock_partition.partition_state_.load();
      });

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));
  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_2_id, PartitionType::Local, "https://localhost"}));
  EXPECT_THAT(partition_manager_.Stop(),
              ResultIs(FailureExecutionResult(1234)));
}

TEST_F(PBSPartitionManagerTest, DoubleLoadPartitionFailsIfFirstSucceeds) {
  EXPECT_CALL(*mock_partition_1, Init)
      .WillOnce([&mock_partition = *mock_partition_1]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Initialized;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition_1, Load)
      .WillOnce([&mock_partition = *mock_partition_1]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Loaded;
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());
  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));
  EXPECT_THAT(
      partition_manager_.LoadPartition(
          {mock_partition_1_id, PartitionType::Local, "https://localhost"}),
      ResultIs(FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));
}

TEST_F(PBSPartitionManagerTest, DoubleLoadPartitionSuccessIfFirstLoadFails) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  // Partition.Load() fails
  EXPECT_CALL(*mock_partition_1, Init)
      .WillOnce([&mock_partition = *mock_partition_1]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Initialized;
        return SuccessExecutionResult();
      })
      .WillRepeatedly(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_partition_1, Load)
      .WillOnce(Return(FailureExecutionResult(1234)))
      .WillRepeatedly(Return(SuccessExecutionResult()));

  EXPECT_THAT(
      partition_manager_.LoadPartition(
          {mock_partition_1_id, PartitionType::Local, "https://localhost"}),
      ResultIs(
          FailureExecutionResult(core::errors::SC_PBS_PARTITION_LOAD_FAILURE)));
  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));

  // Partition.Init() fails
  EXPECT_CALL(*mock_partition_2, Init)
      .WillOnce(Return(FailureExecutionResult(1234)))
      .WillRepeatedly(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_partition_2, Load)
      .WillOnce([&mock_partition = *mock_partition_2]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Loaded;
        return SuccessExecutionResult();
      })
      .WillRepeatedly(Return(SuccessExecutionResult()));

  EXPECT_THAT(
      partition_manager_.LoadPartition(
          {mock_partition_2_id, PartitionType::Local, "https://localhost"}),
      ResultIs(
          FailureExecutionResult(core::errors::SC_PBS_PARTITION_LOAD_FAILURE)));
  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_2_id, PartitionType::Local, "https://localhost"}));
}

TEST_F(PBSPartitionManagerTest, UnloadPartitionSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));
  EXPECT_SUCCESS(partition_manager_.UnloadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));
}

TEST_F(PBSPartitionManagerTest, DoubleUnloadPartition) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));
  EXPECT_SUCCESS(partition_manager_.UnloadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));
  EXPECT_THAT(
      partition_manager_.UnloadPartition(
          {mock_partition_1_id, PartitionType::Local, "https://localhost"}),
      ResultIs(FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
}

TEST_F(PBSPartitionManagerTest, UnloadPartitionFailsToUnloadPartition) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  EXPECT_CALL(*mock_partition_1, Init)
      .WillOnce([&mock_partition = *mock_partition_1]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Initialized;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition_1, Load)
      .WillOnce([&mock_partition = *mock_partition_1]() {
        mock_partition.partition_state_ =
            core::PartitionLoadUnloadState::Loaded;
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_partition_1, Unload)
      .WillOnce(Return(FailureExecutionResult(1234)));

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));
  EXPECT_THAT(
      partition_manager_.UnloadPartition(
          {mock_partition_1_id, PartitionType::Local, "https://localhost"}),
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_PARTITION_UNLOAD_FAILURE)));
}

TEST_F(PBSPartitionManagerTest, GetPartitionAddress) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));

  EXPECT_TRUE(
      partition_manager_.GetPartitionAddress(mock_partition_1_id).has_value());
  EXPECT_STREQ(partition_manager_.GetPartitionAddress(mock_partition_1_id)
                   .value()
                   ->c_str(),
               "https://localhost");

  EXPECT_SUCCESS(partition_manager_.Stop());
}

TEST_F(PBSPartitionManagerTest, RefreshPartitionAddress) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));

  EXPECT_TRUE(
      partition_manager_.GetPartitionAddress(mock_partition_1_id).has_value());
  EXPECT_STREQ(partition_manager_.GetPartitionAddress(mock_partition_1_id)
                   .value()
                   ->c_str(),
               "https://localhost");

  EXPECT_SUCCESS(partition_manager_.RefreshPartitionAddress(
      {mock_partition_1_id, PartitionType::Local, "https://localhost:80"}));

  EXPECT_TRUE(
      partition_manager_.GetPartitionAddress(mock_partition_1_id).has_value());
  EXPECT_STREQ(partition_manager_.GetPartitionAddress(mock_partition_1_id)
                   .value()
                   ->c_str(),
               "https://localhost:80");

  EXPECT_SUCCESS(partition_manager_.Stop());
}

TEST_F(PBSPartitionManagerTest, GetPartitionType) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));

  EXPECT_TRUE(
      partition_manager_.GetPartitionType(mock_partition_1_id).has_value());
  EXPECT_EQ(*partition_manager_.GetPartitionType(mock_partition_1_id),
            PartitionType::Local);

  EXPECT_SUCCESS(partition_manager_.Stop());
}

TEST_F(PBSPartitionManagerTest, GetPartition) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));

  EXPECT_TRUE(partition_manager_.GetPartition(mock_partition_1_id).has_value());
  EXPECT_EQ(*partition_manager_.GetPartition(mock_partition_1_id),
            mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.Stop());
}

TEST_F(PBSPartitionManagerTest, GetPBSPartition) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  SetupMocksForAllPartitionMethods(mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.LoadPartition(
      {mock_partition_1_id, PartitionType::Local, "https://localhost"}));

  EXPECT_TRUE(
      partition_manager_.GetPBSPartition(mock_partition_1_id).has_value());
  EXPECT_EQ(*partition_manager_.GetPBSPartition(mock_partition_1_id),
            mock_partition_1);

  EXPECT_SUCCESS(partition_manager_.Stop());
}

}  // namespace google::scp::pbs::test
