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

#include <random>

#include "core/common/concurrent_map/src/error_codes.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/partition/mock/pbs_partition_mock.h"
#include "pbs/partition/src/error_codes.h"
#include "pbs/partition/src/pbs_partition.h"
#include "pbs/partition_manager/mock/pbs_partition_manager_with_overrides.h"
#include "pbs/partition_manager/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::PartitionId;
using google::scp::core::PartitionLoadUnloadState;
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
using std::atomic;
using std::make_shared;
using std::memory_order_relaxed;
using std::mt19937;
using std::random_device;
using std::shared_ptr;
using std::thread;
using std::uniform_int_distribution;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using testing::AnyOf;
using testing::Return;

static constexpr ExecutionResult kOtherFailure = FailureExecutionResult(1234);

namespace google::scp::pbs::test {
class PBSPartitionManagerConcurrencyTest : public testing::Test {
 protected:
  PBSPartitionManagerConcurrencyTest()
      : partition_manager_(partition_dependencies_,
                           1000 /* transaction capacity */) {
    partition_manager_.construct_partition_override_ =
        [&](const PartitionId& partition_id,
            const PartitionType& partition_type) {
          auto partition = make_shared<MockPBSPartition>();
          SetupMocksForAllPartitionMethods(partition);
          return partition;
        };
    auto mock_config_provider = make_shared<MockConfigProvider>();
    mock_config_provider->Set(kJournalServiceBucketName, "budget");
    partition_manager_.SetConfigProvider(mock_config_provider);
  }

  milliseconds PickRandomDelayInMilliseconds(size_t max_milliseconds) {
    static random_device random_device_local;
    static mt19937 random_generator(random_device_local());
    static uniform_int_distribution<uint64_t> distribution;
    // single digit delay
    return milliseconds(distribution(random_generator) % max_milliseconds);
  }

  void SetupMocksForAllPartitionMethods(
      shared_ptr<MockPBSPartition> mock_partition) {
    // Partition 1
    EXPECT_CALL(*mock_partition, Init)
        .WillRepeatedly([mock_partition_ptr = &(*mock_partition)]() {
          mock_partition_ptr->partition_state_ =
              core::PartitionLoadUnloadState::Initialized;
          return SuccessExecutionResult();
        });
    EXPECT_CALL(*mock_partition, Load)
        .WillRepeatedly([this, mock_partition_ptr = &(*mock_partition)]() {
          auto current_state = PartitionLoadUnloadState::Initialized;
          if (!mock_partition_ptr->partition_state_.compare_exchange_strong(
                  current_state, PartitionLoadUnloadState::Loading)) {
            return kOtherFailure;
          }

          sleep_for(milliseconds(PickRandomDelayInMilliseconds(50)));

          current_state = PartitionLoadUnloadState::Loading;
          if (!mock_partition_ptr->partition_state_.compare_exchange_strong(
                  current_state, PartitionLoadUnloadState::Loaded)) {
            return kOtherFailure;
          }
          return SuccessExecutionResult();
        });
    EXPECT_CALL(*mock_partition, Unload)
        .WillRepeatedly([this, mock_partition_ptr = &(*mock_partition)]() {
          auto current_state = PartitionLoadUnloadState::Loaded;
          if (!mock_partition_ptr->partition_state_.compare_exchange_strong(
                  current_state, PartitionLoadUnloadState::Unloading)) {
            return kOtherFailure;
          }

          sleep_for(milliseconds(PickRandomDelayInMilliseconds(50)));

          current_state = PartitionLoadUnloadState::Unloading;
          if (!mock_partition_ptr->partition_state_.compare_exchange_strong(
                  current_state, PartitionLoadUnloadState::Unloaded)) {
            return kOtherFailure;
          }
          return SuccessExecutionResult();
        });
    EXPECT_CALL(*mock_partition, GetPartitionState)
        .WillRepeatedly([mock_partition_ptr = &(*mock_partition)]() {
          return mock_partition_ptr->partition_state_.load();
        });
  }

  PBSPartition::Dependencies partition_dependencies_;
  PBSPartitionManagerWithOverrides partition_manager_;
};

TEST_F(PBSPartitionManagerConcurrencyTest,
       ConcurrentLoadUnloadSamePartitionIsSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  PartitionId partition{1, 2};
  atomic<bool> start(false);
  int num_threads = 32;
  atomic<size_t> loads_succeeded(0);

  vector<thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back([&]() {
      while (!start) {}
      size_t times_to_load_unload = 5000;
      for (int i = 0; i < times_to_load_unload; i++) {
        auto execution_result = partition_manager_.LoadPartition(
            {partition, PartitionType::Local, "https://localhost"});
        if (execution_result.Successful()) {
          EXPECT_SUCCESS(partition_manager_.UnloadPartition(
              {partition, PartitionType::Local, "https://localhost"}));
          loads_succeeded.fetch_add(1, memory_order_relaxed);
        } else {
          EXPECT_THAT(
              execution_result,
              ResultIs(FailureExecutionResult(
                  core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));
        }
      }
    });
  }
  start = true;

  for (int i = 0; i < num_threads; i++) {
    threads[i].join();
  }

  EXPECT_GE(loads_succeeded, 1);
  EXPECT_SUCCESS(partition_manager_.Stop());
}

TEST_F(PBSPartitionManagerConcurrencyTest,
       StopDuringConcurrentLoadUnloadIsSuccess) {
  EXPECT_SUCCESS(partition_manager_.Init());
  EXPECT_SUCCESS(partition_manager_.Run());

  atomic<bool> start(false);
  int num_threads = 32;

  vector<thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back([&]() {
      while (!start) {}
      size_t times_to_load_unload = 5000;
      PartitionId partition = Uuid::GenerateUuid();
      for (int i = 0; i < times_to_load_unload; i++) {
        auto execution_result = partition_manager_.LoadPartition(
            {partition, PartitionType::Local, "https://localhost"});
        if (execution_result.Successful()) {
          execution_result = partition_manager_.UnloadPartition(
              {partition, PartitionType::Local, "https://localhost"});
          if (!execution_result.Successful()) {
            // Cannot unload occurs if another thread is concurrently
            // unloading.
            EXPECT_THAT(
                execution_result,
                AnyOf(
                    ResultIs(FailureExecutionResult(
                        core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)),
                    ResultIs(FailureExecutionResult(
                        core::errors::SC_PBS_PARTITION_UNLOAD_FAILURE)),
                    ResultIs(FailureExecutionResult(
                        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING))));
          }
        } else {
          // Already exits occurs if another thread is concurrently
          // loading.
          EXPECT_THAT(
              execution_result,
              AnyOf(ResultIs(FailureExecutionResult(
                        core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)),
                    ResultIs(FailureExecutionResult(
                        core::errors::SC_PBS_PARTITION_MANAGER_NOT_RUNNING))));
        }
      }
    });
  }
  start = true;

  sleep_for(PickRandomDelayInMilliseconds(15));

  EXPECT_SUCCESS(partition_manager_.Stop());

  for (int i = 0; i < num_threads; i++) {
    threads[i].join();
  }
}

}  // namespace google::scp::pbs::test
