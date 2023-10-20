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

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/lease_manager/mock/mock_leasable_lock_gmock.h"
#include "core/lease_manager/mock/mock_lease_event_sink.h"
#include "core/lease_manager/src/v2/error_codes.h"
#include "core/lease_manager/src/v2/lease_refresher_factory.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::lease_manager::mock::MockLeasableLock;
using google::scp::core::lease_manager::mock::MockLeaseEventSink;
using std::abort;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::string;
using std::unique_lock;
using std::chrono::milliseconds;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Optional;
using ::testing::Return;

namespace google::scp::core::test {

TEST(LeaseRefresherFactoryTest, ConstructLeaseRefresherInstanceFromFactory) {
  std::shared_ptr<LeasableLockInterface> lock =
      std::make_shared<MockLeasableLock>();
  std::shared_ptr<LeaseEventSinkInterface> sink =
      std::make_shared<MockLeaseEventSink>();
  LeasableLockId id = {2, 3};
  LeaseRefresherFactory factory;
  auto refresher = factory.Construct(id, lock, sink);
  EXPECT_NE(refresher.get(), nullptr);
}

class LeaseRefresherTest : public ::testing::Test {
 protected:
  LeaseRefresherTest() {
    mock_lock_ = std::make_shared<MockLeasableLock>();
    lock_ = mock_lock_;
    mock_event_sink_ = std::make_shared<MockLeaseEventSink>();
    event_sink_ = mock_event_sink_;
    refresher_ = factory_.Construct(lock_id_, lock_, event_sink_);
    refresher_liveness_check_ =
        std::dynamic_pointer_cast<LeaseRefreshLivenessCheckInterface>(
            refresher_);
    EXPECT_SUCCESS(refresher_->Init());

    // Set up mock calls for mock_lock_
    ON_CALL(*mock_lock_, ShouldRefreshLease()).WillByDefault([this]() {
      return should_refresh_lease_.load();
    });
    ON_CALL(*mock_lock_, RefreshLease(_))
        .WillByDefault(Return(SuccessExecutionResult()));
    ON_CALL(*mock_lock_, GetCurrentLeaseOwnerInfo()).WillByDefault([this]() {
      return lease_holder_info_;
    });
    ON_CALL(*mock_lock_, IsCurrentLeaseOwner()).WillByDefault(Return(false));

    // Set up mock calls for the mock_event_sink
    ON_CALL(*mock_event_sink_, OnLeaseTransition(_, _, _))
        .WillByDefault(Return());
  }

  LeasableLockId lock_id_ = {2, 3};
  std::shared_ptr<MockLeasableLock> mock_lock_;
  std::shared_ptr<LeasableLockInterface> lock_;
  std::shared_ptr<MockLeaseEventSink> mock_event_sink_;
  std::shared_ptr<LeaseEventSinkInterface> event_sink_;
  std::shared_ptr<LeaseRefresherInterface> refresher_;
  std::shared_ptr<LeaseRefreshLivenessCheckInterface> refresher_liveness_check_;
  LeaseRefresherFactory factory_;

  std::atomic<bool> should_refresh_lease_ = true;
  LeaseInfo lease_holder_info_ = {"lease_acquirer_id", "1.1.1.1"};
};

TEST_F(LeaseRefresherTest, RunStop) {
  EXPECT_SUCCESS(refresher_->Run());
  EXPECT_SUCCESS(refresher_->Stop());
}

TEST_F(LeaseRefresherTest, RefreshesLockPeriodically) {
  std::atomic<bool> is_invoked;
  EXPECT_CALL(*mock_lock_, RefreshLease(_))
      .WillOnce(Return(SuccessExecutionResult()))
      .WillOnce(Return(SuccessExecutionResult()))
      .WillRepeatedly([&is_invoked](bool) {
        is_invoked = true;
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(refresher_->Run());
  WaitUntil([&is_invoked]() { return is_invoked.load(); });
  EXPECT_SUCCESS(refresher_->Stop());
}

TEST_F(LeaseRefresherTest,
       RefreshesLockPeriodicallyAndUpdatesRefreshTimestamp) {
  EXPECT_SUCCESS(refresher_->Run());
  auto previous_timestamp =
      refresher_liveness_check_->GetLastLeaseRefreshTimestamp();
  WaitUntil([this, previous_timestamp]() {
    return refresher_liveness_check_->GetLastLeaseRefreshTimestamp() >
           previous_timestamp;
  });
  previous_timestamp =
      refresher_liveness_check_->GetLastLeaseRefreshTimestamp();
  WaitUntil([this, previous_timestamp]() {
    return refresher_liveness_check_->GetLastLeaseRefreshTimestamp() >
           previous_timestamp;
  });
  EXPECT_SUCCESS(refresher_->Stop());
}

TEST_F(LeaseRefresherTest, RefreshesLockPeriodicallyAndInvokesSink) {
  std::atomic<bool> is_invoked(false);
  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, _,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return())
      .WillOnce(Return())
      .WillOnce(Return())
      .WillRepeatedly([&is_invoked]() { is_invoked = true; });
  EXPECT_SUCCESS(refresher_->Run());
  WaitUntil([&is_invoked]() { return is_invoked.load(); });
  EXPECT_SUCCESS(refresher_->Stop());
}

TEST_F(LeaseRefresherTest, ByDefaultDoesNotAcquireLease) {
  EXPECT_CALL(*mock_lock_, RefreshLease(true))
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kNotAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());
}

TEST_F(LeaseRefresherTest, LeaseIsRefreshedWithReadOnlyByDefault) {
  EXPECT_CALL(*mock_lock_, RefreshLease(true))
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());
}

TEST_F(LeaseRefresherTest,
       LeaseIsRefreshedWithWriteWhenRefreshWithIntentionToHoldLease) {
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease));
  EXPECT_CALL(*mock_lock_, RefreshLease(false))
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());
}

TEST_F(LeaseRefresherTest,
       LeaseIsRefreshedWithWriteWhenRefreshWithIntentionToReleaseTheHeldLease) {
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease));
  EXPECT_CALL(*mock_lock_, RefreshLease(false))
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());
}

TEST_F(LeaseRefresherTest,
       LeaseIsRefreshedWithReadOnlyWhenRefreshWithNoIntentionToHoldLease) {
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithNoIntentionToHoldLease));
  EXPECT_CALL(*mock_lock_, RefreshLease(true))
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());
}

TEST_F(LeaseRefresherTest, AcquireRenewLostEventsOnSink) {
  ON_CALL(*mock_lock_, IsCurrentLeaseOwner()).WillByDefault([this]() {
    return ((refresher_->GetLeaseRefreshMode() ==
             LeaseRefreshMode::RefreshWithIntentionToHoldLease) ||
            (refresher_->GetLeaseRefreshMode() ==
             LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease));
  });

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kNotAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  // Change mode to indicate the intention to hold the lease
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease));

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kRenewed,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  // Change mode to indicate the intention to not hold the lease
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithNoIntentionToHoldLease));

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kLost,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kNotAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());
}

TEST_F(LeaseRefresherTest, AcquireRenewReleaseAcquireEventsOnSink) {
  ON_CALL(*mock_lock_, IsCurrentLeaseOwner()).WillByDefault([this]() {
    return ((refresher_->GetLeaseRefreshMode() ==
             LeaseRefreshMode::RefreshWithIntentionToHoldLease) ||
            (refresher_->GetLeaseRefreshMode() ==
             LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease));
  });

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kNotAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  // Change mode to indicate the intention to release instead of losing the
  // lease.
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease));

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kRenewed,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  // Change mode to indicate the intention to release instead of losing the
  // lease.
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease));

  EXPECT_CALL(*mock_event_sink_,
              OnLeaseTransition(
                  lock_id_, LeaseTransitionType::kRenewedWithIntentionToRelease,
                  Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  // Change mode to indicate the no intention to acquire release. This should
  // send out a released transition.
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithNoIntentionToHoldLease));

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kReleased,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  // Intention to acquire lease cannot be performed at this point. It can be
  // performed only after the lease transitions to not acquired.
  EXPECT_THAT(
      refresher_->SetLeaseRefreshMode(
          LeaseRefreshMode::RefreshWithIntentionToHoldLease),
      ResultIs(FailureExecutionResult(
          core::errors::
              SC_LEASE_REFRESHER_INVALID_STATE_WHEN_SETTING_REFRESH_MODE)));

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kNotAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());

  // Change the mode to indicate the intention to acquire lease.
  EXPECT_SUCCESS(refresher_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease));

  EXPECT_CALL(
      *mock_event_sink_,
      OnLeaseTransition(lock_id_, LeaseTransitionType::kAcquired,
                        Optional(LeaseInfo{"lease_acquirer_id", "1.1.1.1"})))
      .WillOnce(Return());
  EXPECT_SUCCESS(refresher_->PerformLeaseRefresh());
}

}  // namespace google::scp::core::test
