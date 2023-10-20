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

#include "pbs/partition_lease_event_sink/src/partition_lease_event_sink.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/partition_manager_interface.h"
#include "core/lease_manager/mock/mock_lease_release_notification.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "pbs/partition_manager/mock/pbs_partition_manager_mock.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseReleaseNotificationInterface;
using google::scp::core::LeaseTransitionType;
using google::scp::core::PartitionId;
using google::scp::core::PartitionManagerInterface;
using google::scp::core::PartitionMetadata;
using google::scp::core::PartitionType;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::lease_manager::mock::
    MockLeaseReleaseNotificationInterface;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::partition_manager::mock::MockPBSPartitionManager;
using std::atomic;
using std::function;
using std::make_shared;
using std::nullopt;
using std::shared_ptr;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;
using ::testing::Return;

namespace google::scp::pbs::test {
class PartitionLeaseEventSinkTest : public ::testing::Test {
 protected:
  PartitionLeaseEventSinkTest() {
    mock_pbs_partition_manager_ = make_shared<MockPBSPartitionManager>();
    partition_manager_ = mock_pbs_partition_manager_;

    mock_lease_release_notification_ =
        make_shared<MockLeaseReleaseNotificationInterface>();
    lease_release_notification_ = mock_lease_release_notification_;

    auto mock_async_executor = make_shared<MockAsyncExecutor>();
    mock_async_executor->schedule_for_mock =
        [](const core::AsyncOperation& work, core::Timestamp,
           function<bool()>& cancellation_lambda) {
          cancellation_lambda = []() { return false; };
          work();
          return SuccessExecutionResult();
        };

    metric_client_ = make_shared<MockMetricClient>();

    config_provider_ = make_shared<MockConfigProvider>();

    // The async executor is used for metrics only.
    async_executor_ = make_shared<AsyncExecutor>(2 /** threads count */,
                                                 10000 /* queue cap */);
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());

    core::test::TestLoggingUtils::EnableLogOutputToConsole();
  }

  ~PartitionLeaseEventSinkTest() { EXPECT_SUCCESS(async_executor_->Stop()); }

  shared_ptr<AsyncExecutorInterface> async_executor_;

  shared_ptr<MockPBSPartitionManager> mock_pbs_partition_manager_;
  shared_ptr<PartitionManagerInterface> partition_manager_;

  shared_ptr<MockLeaseReleaseNotificationInterface>
      mock_lease_release_notification_;
  shared_ptr<LeaseReleaseNotificationInterface> lease_release_notification_;

  shared_ptr<MetricClientInterface> metric_client_;
  shared_ptr<ConfigProviderInterface> config_provider_;

  atomic<size_t> abort_called_ = {0};
  function<void()> abort_handler_ = [this]() { abort_called_++; };

  PartitionId partition_id_1_ = {1, 1};
  PartitionId partition_id_2_ = {2, 2};
};

// Scenario 1:
// The test verifies the intended behavior on Partition Manager upon sending
// a) LeaseTransitionType::kNotAcquired,
// b) LeaseTransitionType::kAcquired,
// c) LeaseTransitionType::kRenewed,
// d) LeaseTransitionType::kLost,
// e) LeaseTransitionType::kNotAcquired,
// events in a sequence to the PartitionLeaseEventSink.
//
TEST_F(PartitionLeaseEventSinkTest, TestLeaseAcquireLostScenarioWorks) {
  PartitionLeaseEventSink sink(partition_manager_, async_executor_,
                               lease_release_notification_, metric_client_,
                               config_provider_, seconds(1), abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  // a) No partition exists to start with.
  auto remote_owner_endpoint_uri = "https:://1.1.1.1:8080";
  auto remote_owner_id = "remote_owner_id";
  EXPECT_CALL(
      *mock_pbs_partition_manager_,
      RefreshPartitionAddress(PartitionMetadata{
          partition_id_1_, PartitionType::Remote, remote_owner_endpoint_uri}))
      .WillOnce(Return(FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));

  EXPECT_CALL(
      *mock_pbs_partition_manager_,
      LoadPartition(PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                      remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kNotAcquired,
                         LeaseInfo{remote_owner_id, remote_owner_endpoint_uri});

  // b) Lease acquired.
  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_pbs_partition_manager_,
              LoadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);

  // Wait for a couple of seconds for the load task to finish
  sleep_for(seconds(2));

  // c) Lease renewed (several times)
  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kRenewed,
                         nullopt /* nullopt because this is the owner */);
  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kRenewed,
                         nullopt /* nullopt because this is the owner */);
  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kRenewed,
                         nullopt /* nullopt because this is the owner */);

  // d) Lease lost
  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kLost,
                         nullopt /* nullopt because this is the owner */);

  // e) Lease not acquired
  auto another_remote_owner_endpoint_uri = "https:://1.1.1.1";
  auto another_remote_owner_id = "another_remote_owner_id";
  EXPECT_CALL(*mock_pbs_partition_manager_,
              RefreshPartitionAddress(
                  PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                    another_remote_owner_endpoint_uri}))
      .WillOnce(Return(FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));

  EXPECT_CALL(
      *mock_pbs_partition_manager_,
      LoadPartition(PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                      another_remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(
      partition_id_1_, LeaseTransitionType::kNotAcquired,
      LeaseInfo{another_remote_owner_id, another_remote_owner_endpoint_uri});

  // e) Lease not acquired again, but remote owner's address changed.
  another_remote_owner_endpoint_uri = "https:://2.2.2.2:8080";
  EXPECT_CALL(*mock_pbs_partition_manager_,
              RefreshPartitionAddress(
                  PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                    another_remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(
      partition_id_1_, LeaseTransitionType::kNotAcquired,
      LeaseInfo{another_remote_owner_id, another_remote_owner_endpoint_uri});

  // e) Lease not acquired again, but remote owner's address unchanged.
  EXPECT_CALL(*mock_pbs_partition_manager_,
              RefreshPartitionAddress(
                  PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                    another_remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(
      partition_id_1_, LeaseTransitionType::kNotAcquired,
      LeaseInfo{another_remote_owner_id, another_remote_owner_endpoint_uri});

  EXPECT_EQ(abort_called_, 0);

  EXPECT_SUCCESS(sink.Stop());
}

// Scenario 2:
// The test verifies the intended behavior on Partition Manager upon sending
// a) LeaseTransitionType::kNotAcquired,
// b) LeaseTransitionType::kAcquired,
// c) LeaseTransitionType::kRenewed,
// d) LeaseTransitionType::kRenewedWithIntentionToRelease,
// e) LeaseTransitionType::kReleased,
// f) LeaseTransitionType::kNotAcquired,
// events in a sequence to the PartitionLeaseEventSink.
//
TEST_F(PartitionLeaseEventSinkTest, TestLeaseAcquireReleaseScenarioWorks) {
  PartitionLeaseEventSink sink(partition_manager_, async_executor_,
                               lease_release_notification_, metric_client_,
                               config_provider_, seconds(1), abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  // a) No partition exists to start with.
  auto remote_owner_endpoint_uri = "https:://1.1.1.1:8080";
  auto remote_owner_id = "remote_owner_id";
  EXPECT_CALL(
      *mock_pbs_partition_manager_,
      RefreshPartitionAddress(PartitionMetadata{
          partition_id_1_, PartitionType::Remote, remote_owner_endpoint_uri}))
      .WillOnce(Return(FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));

  EXPECT_CALL(
      *mock_pbs_partition_manager_,
      LoadPartition(PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                      remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kNotAcquired,
                         LeaseInfo{remote_owner_id, remote_owner_endpoint_uri});

  // b) Lease acquired.
  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_pbs_partition_manager_,
              LoadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);

  // Wait for some time for the load task to start
  sleep_for(seconds(2));

  // c) Lease renewed (several times)
  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kRenewed,
                         nullopt /* nullopt because this is the owner */);
  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kRenewed,
                         nullopt /* nullopt because this is the owner */);
  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kRenewed,
                         nullopt /* nullopt because this is the owner */);

  // d) Lease renewed with intention to release.
  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_lease_release_notification_,
              SafeToReleaseLease(partition_id_1_))
      .WillOnce(Return());

  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);

  // Wait for some time for the unload task to finish
  sleep_for(seconds(2));

  // e) Lease released
  auto another_remote_owner_endpoint_uri = "https:://1.1.1.1";
  auto another_remote_owner_id = "another_remote_owner_id";
  sink.OnLeaseTransition(
      partition_id_1_, LeaseTransitionType::kReleased,
      LeaseInfo{another_remote_owner_id, another_remote_owner_endpoint_uri});

  // f) Lease not acquired
  EXPECT_CALL(*mock_pbs_partition_manager_,
              RefreshPartitionAddress(
                  PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                    another_remote_owner_endpoint_uri}))
      .WillOnce(Return(FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));

  EXPECT_CALL(
      *mock_pbs_partition_manager_,
      LoadPartition(PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                      another_remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(
      partition_id_1_, LeaseTransitionType::kNotAcquired,
      LeaseInfo{remote_owner_id, another_remote_owner_endpoint_uri});

  // e) Lease not acquired again, but remote owner's address changed.
  another_remote_owner_endpoint_uri = "https:://1.1.1.1:8080";
  EXPECT_CALL(*mock_pbs_partition_manager_,
              RefreshPartitionAddress(
                  PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                    another_remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(
      partition_id_1_, LeaseTransitionType::kNotAcquired,
      LeaseInfo{remote_owner_id, another_remote_owner_endpoint_uri});

  // e) Lease not acquired again, but remote owner's address unchanged.
  EXPECT_CALL(*mock_pbs_partition_manager_,
              RefreshPartitionAddress(
                  PartitionMetadata{partition_id_1_, PartitionType::Remote,
                                    another_remote_owner_endpoint_uri}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(
      partition_id_1_, LeaseTransitionType::kNotAcquired,
      LeaseInfo{remote_owner_id, another_remote_owner_endpoint_uri});

  EXPECT_EQ(abort_called_, 0);

  EXPECT_SUCCESS(sink.Stop());
}

TEST_F(PartitionLeaseEventSinkTest, PendingTaskLeadsToAbortWhenLeaseIsLost) {
  PartitionLeaseEventSink sink(
      partition_manager_, async_executor_, lease_release_notification_,
      metric_client_, config_provider_, seconds(0) /* execute immediately */,
      abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  atomic<bool> should_wait(true);
  EXPECT_CALL(*mock_pbs_partition_manager_,
              LoadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce([&should_wait](PartitionMetadata) {
        // Load takes forever..
        while (should_wait) {
          sleep_for(milliseconds(100));
        }
        return SuccessExecutionResult();
      });

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  // Wait until the task starts.
  sleep_for(seconds(1));

  // At this point, the load is blocked in the task. The Lost event handling
  // cannot proceed.
  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kLost,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 1);

  should_wait = false;

  EXPECT_SUCCESS(sink.Stop());
}

TEST_F(PartitionLeaseEventSinkTest,
       PendingNotRunningTaskIsCancelledAndUnloadCanProceedDuringLeaseLost) {
  PartitionLeaseEventSink sink(
      partition_manager_, async_executor_, lease_release_notification_,
      metric_client_, config_provider_,
      seconds(10) /* execute the task after 10 seconds */, abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kLost,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  EXPECT_SUCCESS(sink.Stop());
}

TEST_F(PartitionLeaseEventSinkTest,
       PendingNotRunningTaskIsCancelledAndUnloadCanProceedDuringLeaseRelease) {
  PartitionLeaseEventSink sink(
      partition_manager_, async_executor_, lease_release_notification_,
      metric_client_, config_provider_,
      seconds(10) /* execute the task after 10 seconds */, abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  atomic<bool> unload_done = false;
  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce([&unload_done]() {
        unload_done = true;
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*mock_lease_release_notification_,
              SafeToReleaseLease(partition_id_1_))
      .WillOnce(Return());

  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);
  WaitUntil([&unload_done]() { return unload_done.load(); });

  EXPECT_SUCCESS(sink.Stop());
}

TEST_F(PartitionLeaseEventSinkTest,
       PendingLoadTaskLeadsToTaskCancellationWhenLeaseIsRenewedWithRelease) {
  PartitionLeaseEventSink sink(partition_manager_, async_executor_,
                               lease_release_notification_, metric_client_,
                               config_provider_, seconds(15), abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  // At this point, the load hasn't gotten a chance to execute because of a
  // start up delay of 15 seconds, so unloading can proceed.
  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);

  // Wait until the task starts.
  sleep_for(seconds(1));

  EXPECT_EQ(abort_called_, 0);

  EXPECT_SUCCESS(sink.Stop());
}

TEST_F(PartitionLeaseEventSinkTest,
       OngoingLoadTaskLeadsToNoOpWhenLeaseIsRenewedWithRelease) {
  PartitionLeaseEventSink sink(
      partition_manager_, async_executor_, lease_release_notification_,
      metric_client_, config_provider_, seconds(0) /* execute immediately */,
      abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  atomic<bool> should_wait(true);
  EXPECT_CALL(*mock_pbs_partition_manager_,
              LoadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce([&should_wait](PartitionMetadata) {
        // Load takes forever..
        while (should_wait) {
          sleep_for(milliseconds(100));
        }
        return SuccessExecutionResult();
      });

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  // Wait until the task starts.
  sleep_for(seconds(1));

  // At this point, the load is blocked in the task. The Lost event handling
  // cannot proceed.
  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  // lease renewed...
  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  // lease renewed...
  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  should_wait = false;

  EXPECT_SUCCESS(sink.Stop());
}

TEST_F(PartitionLeaseEventSinkTest,
       NoAbortOnTransitionWhenUnloadTaskDuringRelease) {
  PartitionLeaseEventSink sink(
      partition_manager_, async_executor_, lease_release_notification_,
      metric_client_, config_provider_, seconds(0) /* execute immediately */,
      abort_handler_);
  EXPECT_SUCCESS(sink.Init());
  EXPECT_SUCCESS(sink.Run());

  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(PartitionMetadata{partition_id_1_,
                                                PartitionType::Remote, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_pbs_partition_manager_,
              LoadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce(Return(SuccessExecutionResult()));

  sink.OnLeaseTransition(partition_id_1_, LeaseTransitionType::kAcquired,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  atomic<bool> should_wait(true);
  EXPECT_CALL(*mock_pbs_partition_manager_,
              UnloadPartition(
                  PartitionMetadata{partition_id_1_, PartitionType::Local, ""}))
      .WillOnce([&should_wait](PartitionMetadata) {
        // Load takes forever..
        while (should_wait) {
          sleep_for(milliseconds(100));
        }
        return SuccessExecutionResult();
      });

  // Wait until the task starts.
  sleep_for(seconds(1));

  // The unload will be blocked in the task.
  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  // Wait until the task starts.
  sleep_for(seconds(1));

  sink.OnLeaseTransition(partition_id_1_,
                         LeaseTransitionType::kRenewedWithIntentionToRelease,
                         nullopt /* nullopt because this is the owner */);
  EXPECT_EQ(abort_called_, 0);

  should_wait = false;

  EXPECT_SUCCESS(sink.Stop());
}

}  // namespace google::scp::pbs::test
