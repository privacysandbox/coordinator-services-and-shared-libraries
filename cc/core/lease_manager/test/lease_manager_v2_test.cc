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

#include "core/lease_manager/src/v2/lease_manager_v2.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/lease_manager_interface.h"
#include "core/lease_manager/mock/mock_leasable_lock_gmock.h"
#include "core/lease_manager/mock/mock_lease_event_sink.h"
#include "core/lease_manager/mock/mock_lease_refresher.h"
#include "core/lease_manager/mock/mock_lease_refresher_factory.h"
#include "core/lease_manager/src/v2/error_codes.h"
#include "core/lease_manager/src/v2/lease_refresh_liveness_enforcer.h"
#include "core/lease_manager/src/v2/lease_refresher_factory.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::common::TimeProvider;
using google::scp::core::lease_manager::mock::MockLeasableLock;
using google::scp::core::lease_manager::mock::MockLeaseEventSink;
using google::scp::core::lease_manager::mock::MockLeaseRefresher;
using google::scp::core::lease_manager::mock::MockLeaseRefresherFactory;
using google::scp::core::lease_manager::mock::MockLeaseRefreshLivenessCheck;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::mutex;
using std::nullopt;
using std::optional;
using std::shared_ptr;
using std::unique_lock;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using ::testing::_;
using ::testing::Optional;
using ::testing::Return;

namespace google::scp::core::test {

static constexpr size_t kSecondInMillis = 1000;

class LeaseManagerV2Test : public ::testing::Test {
 protected:
  void SetUpMocksForLock(shared_ptr<MockLeasableLock> mock_lock,
                         optional<LeaseInfo> current_lease_owner = nullopt) {
    // Set up mock calls for mock_lock_
    ON_CALL(*mock_lock, ShouldRefreshLease()).WillByDefault(Return(true));
    ON_CALL(*mock_lock, RefreshLease(_))
        .WillByDefault(Return(SuccessExecutionResult()));
    ON_CALL(*mock_lock, GetCurrentLeaseOwnerInfo())
        .WillByDefault(Return(current_lease_owner));
    ON_CALL(*mock_lock, GetConfiguredLeaseDurationInMilliseconds())
        .WillByDefault(Return(10 * kSecondInMillis));
    ON_CALL(*mock_lock, IsCurrentLeaseOwner())
        .WillByDefault(
            Return(current_lease_owner.has_value() &&
                   (current_lease_owner.value() == this_lease_holder_info_)));
  }

  void SetUpMocksForEventSink(shared_ptr<MockLeaseEventSink> mock_event_sink) {
    // Set up mock calls for the mock_event_sink
    ON_CALL(*mock_event_sink, OnLeaseTransition(_, _, _))
        .WillByDefault(Return());
  }

  size_t GetRefresherSetCount() {
    return mock_lease_refresher_1_->refresh_counter_ +
           mock_lease_refresher_2_->refresh_counter_ +
           mock_lease_refresher_3_->refresh_counter_;
  }

  size_t GetRefresherSetCountForAMode(LeaseRefreshMode mode) {
    size_t count = 0;
    if (mock_lease_refresher_1_->GetLeaseRefreshMode() == mode) {
      count++;
    }
    if (mock_lease_refresher_2_->GetLeaseRefreshMode() == mode) {
      count++;
    }
    if (mock_lease_refresher_3_->GetLeaseRefreshMode() == mode) {
      count++;
    }
    return count;
  }

  vector<LeasableLockId> GetLockSetForAMode(LeaseRefreshMode mode) {
    vector<LeasableLockId> locks;
    if (mock_lease_refresher_1_->GetLeaseRefreshMode() == mode) {
      locks.push_back(lock_id_1_);
    }
    if (mock_lease_refresher_2_->GetLeaseRefreshMode() == mode) {
      locks.push_back(lock_id_2_);
    }
    if (mock_lease_refresher_3_->GetLeaseRefreshMode() == mode) {
      locks.push_back(lock_id_3_);
    }
    return locks;
  }

  LeaseManagerV2Test() {
    mock_leasable_lock_1_ = make_shared<MockLeasableLock>();
    leasable_lock_1_ = mock_leasable_lock_1_;
    mock_event_sink_1_ = make_shared<MockLeaseEventSink>();
    event_sink_1_ = mock_event_sink_1_;
    mock_lease_refresher_1_ = make_shared<MockLeaseRefresher>();
    SetUpMocksForLock(mock_leasable_lock_1_);
    SetUpMocksForEventSink(mock_event_sink_1_);

    mock_leasable_lock_2_ = make_shared<MockLeasableLock>();
    leasable_lock_2_ = mock_leasable_lock_2_;
    mock_event_sink_2_ = make_shared<MockLeaseEventSink>();
    event_sink_2_ = mock_event_sink_2_;
    mock_lease_refresher_2_ = make_shared<MockLeaseRefresher>();
    SetUpMocksForLock(mock_leasable_lock_2_);
    SetUpMocksForEventSink(mock_event_sink_2_);

    mock_leasable_lock_3_ = make_shared<MockLeasableLock>();
    leasable_lock_3_ = mock_leasable_lock_3_;
    mock_event_sink_3_ = make_shared<MockLeaseEventSink>();
    event_sink_3_ = mock_event_sink_3_;
    mock_lease_refresher_3_ = make_shared<MockLeaseRefresher>();
    SetUpMocksForLock(mock_leasable_lock_3_);
    SetUpMocksForEventSink(mock_event_sink_3_);

    lease_refresher_factory_ = make_unique<LeaseRefresherFactory>();
    mock_lease_refresher_factory_ = make_unique<MockLeaseRefresherFactory>();

    // Construct a mock lease refresher
    ON_CALL(*mock_lease_refresher_factory_, Construct)
        .WillByDefault([this](const LeasableLockId& lock_id,
                              const shared_ptr<LeasableLockInterface>&,
                              const shared_ptr<LeaseEventSinkInterface>&) {
          if (lock_id_1_ == lock_id) {
            return mock_lease_refresher_1_;
          } else if (lock_id_2_ == lock_id) {
            return mock_lease_refresher_2_;
          } else if (lock_id_3_ == lock_id) {
            return mock_lease_refresher_3_;
          } else {
            throw;
          }
        });
  }

  LeasableLockId lock_id_1_ = {1, 1};

  shared_ptr<MockLeasableLock> mock_leasable_lock_1_;
  shared_ptr<LeasableLockInterface> leasable_lock_1_;
  shared_ptr<MockLeaseEventSink> mock_event_sink_1_;
  shared_ptr<LeaseEventSinkInterface> event_sink_1_;
  shared_ptr<MockLeaseRefresher> mock_lease_refresher_1_;

  LeasableLockId lock_id_2_ = {2, 2};
  shared_ptr<MockLeasableLock> mock_leasable_lock_2_;
  shared_ptr<LeasableLockInterface> leasable_lock_2_;
  shared_ptr<MockLeaseEventSink> mock_event_sink_2_;
  shared_ptr<LeaseEventSinkInterface> event_sink_2_;
  shared_ptr<MockLeaseRefresher> mock_lease_refresher_2_;

  LeasableLockId lock_id_3_ = {3, 3};
  shared_ptr<MockLeasableLock> mock_leasable_lock_3_;
  shared_ptr<LeasableLockInterface> leasable_lock_3_;
  shared_ptr<MockLeaseEventSink> mock_event_sink_3_;
  shared_ptr<LeaseEventSinkInterface> event_sink_3_;
  shared_ptr<MockLeaseRefresher> mock_lease_refresher_3_;

  unique_ptr<LeaseRefresherFactoryInterface> lease_refresher_factory_;
  unique_ptr<MockLeaseRefresherFactory> mock_lease_refresher_factory_;
  LeaseInfo this_lease_holder_info_ = {"my_id", "1.1.1.1"};
};

TEST_F(LeaseManagerV2Test, InitRunStopWithOutAnyLocksAdded) {
  LeaseManagerV2 lease_manager(move(lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_SUCCESS(lease_manager.Run());
  EXPECT_SUCCESS(lease_manager.Stop());
}

TEST_F(LeaseManagerV2Test, InitRunStopWithThreeLocksAdded) {
  LeaseManagerV2 lease_manager(move(lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_SUCCESS(lease_manager.Run());
  EXPECT_SUCCESS(lease_manager.Stop());
}

TEST_F(LeaseManagerV2Test, ThreeLocksRefreshLeasesAndInvokeSink) {
  LeaseManagerV2 lease_manager(move(lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_SUCCESS(lease_manager.Run());

  EXPECT_CALL(*mock_leasable_lock_1_, RefreshLease(_))
      .WillRepeatedly(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_leasable_lock_2_, RefreshLease(_))
      .WillRepeatedly(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_leasable_lock_3_, RefreshLease(_))
      .WillRepeatedly(Return(SuccessExecutionResult()));

  atomic<bool> lock_1_sink_invoked_(false);
  atomic<bool> lock_2_sink_invoked_(false);
  atomic<bool> lock_3_sink_invoked_(false);
  EXPECT_CALL(*mock_event_sink_1_, OnLeaseTransition(lock_id_1_, _, _))
      .WillRepeatedly(
          [&lock_1_sink_invoked_]() { lock_1_sink_invoked_ = true; });
  EXPECT_CALL(*mock_event_sink_2_, OnLeaseTransition(lock_id_2_, _, _))
      .WillRepeatedly(
          [&lock_2_sink_invoked_]() { lock_2_sink_invoked_ = true; });
  EXPECT_CALL(*mock_event_sink_3_, OnLeaseTransition(lock_id_3_, _, _))
      .WillRepeatedly(
          [&lock_3_sink_invoked_]() { lock_3_sink_invoked_ = true; });

  WaitUntil([&lock_1_sink_invoked_, &lock_2_sink_invoked_,
             &lock_3_sink_invoked_]() {
    return lock_1_sink_invoked_ && lock_2_sink_invoked_ && lock_3_sink_invoked_;
  });
  EXPECT_SUCCESS(lease_manager.Stop());
}

TEST_F(LeaseManagerV2Test, RefreshModeDoesNotChangeToBeginWith) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));

  EXPECT_SUCCESS(lease_manager.Init());

  EXPECT_CALL(*mock_lease_refresher_1_, SetLeaseRefreshMode(_)).Times(0);
  EXPECT_CALL(*mock_lease_refresher_2_, SetLeaseRefreshMode(_)).Times(0);
  EXPECT_CALL(*mock_lease_refresher_3_, SetLeaseRefreshMode(_)).Times(0);

  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
}

TEST_F(LeaseManagerV2Test,
       RefreshModeTryToTakeOneLeaseAsSpecifiedInPreference) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{1, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 1);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            1);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeTryToTakeTwoLeasesAsSpecifiedInPreference) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeTryToTakeOneMoreLeaseWhenTwoLeasesAreBeingTried) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);

  // Lease Preference changes from 2->3
  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{3, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 3);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            3);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeTryToTakeOneMoreLeaseWhenTwoLeasesAreBeingHeld) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);

  // Hold leases. There is an order in which the lease_manager chooses locks.
  // Assuming the order here and choosing 2 and 3. This might break if STL
  // changes how unordered_map is organized, but this can be improved to not
  // have ordering baked in, later.
  SetUpMocksForLock(mock_leasable_lock_2_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_3_, this_lease_holder_info_);

  // Lease Preference changes from 2->3
  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{3, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 3);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            3);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeDoesNotAcquireNewIfOneLeaseIsBeingHeldAndOneInProgress) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);

  // Hold leases. There is an order in which the lease_manager chooses locks.
  // Assuming the order here and choosing 3. This might break if STL
  // changes how unordered_map is organized, but this can be improved to not
  // have ordering baked in, later.
  SetUpMocksForLock(mock_leasable_lock_3_, this_lease_holder_info_);

  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeCannotTryTakingLeaseIfAllLeasesHeldByThem) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  SetUpMocksForLock(mock_leasable_lock_1_,
                    LeaseInfo{"another_owner", "1.1.1.1"});
  SetUpMocksForLock(mock_leasable_lock_2_,
                    LeaseInfo{"another_owner", "1.1.1.1"});
  SetUpMocksForLock(mock_leasable_lock_3_,
                    LeaseInfo{"another_owner", "1.1.1.1"});

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 0);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            0);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeTryTakingAvailableLeaseIfSomeLeasesAreHeldByThem) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  SetUpMocksForLock(mock_leasable_lock_1_,
                    LeaseInfo{"another_owner", "1.1.1.1"});
  SetUpMocksForLock(mock_leasable_lock_2_,
                    LeaseInfo{"another_owner", "1.1.1.1"});

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 1);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            1);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeTrySwitchingBackIfSomeLeasesAreAlreadyHeldByThem) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);

  SetUpMocksForLock(mock_leasable_lock_1_,
                    LeaseInfo{"another_owner", "1.1.1.1"});
  SetUpMocksForLock(mock_leasable_lock_2_,
                    LeaseInfo{"another_owner", "1.1.1.1"});
  SetUpMocksForLock(mock_leasable_lock_3_,
                    LeaseInfo{"another_owner", "1.1.1.1"});

  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 4);

  EXPECT_EQ(mock_lease_refresher_1_->GetLeaseRefreshMode(),
            LeaseRefreshMode::RefreshWithNoIntentionToHoldLease);
  EXPECT_EQ(mock_lease_refresher_2_->GetLeaseRefreshMode(),
            LeaseRefreshMode::RefreshWithNoIntentionToHoldLease);
  EXPECT_EQ(mock_lease_refresher_3_->GetLeaseRefreshMode(),
            LeaseRefreshMode::RefreshWithNoIntentionToHoldLease);
}

TEST_F(LeaseManagerV2Test, RefreshModeDoesNotChangeIfLeaseHeldByUs) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();

  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);

  SetUpMocksForLock(mock_leasable_lock_1_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_2_, this_lease_holder_info_);

  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);
}

TEST_F(LeaseManagerV2Test, RefreshModeTryReleaseOneLeaseAsPerPreference) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  // Set up precondition.
  mock_lease_refresher_1_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);
  mock_lease_refresher_2_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);
  mock_lease_refresher_3_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);

  SetUpMocksForLock(mock_leasable_lock_1_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_2_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_3_, this_lease_holder_info_);

  EXPECT_EQ(GetRefresherSetCount(), 3);

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{3, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();

  EXPECT_EQ(GetRefresherSetCount(), 3);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            3);

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{2, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();

  EXPECT_EQ(GetRefresherSetCount(), 4);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease),
            1);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);

  // Another run does not change mode.
  EXPECT_EQ(GetRefresherSetCount(), 4);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease),
            1);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            2);
}

TEST_F(LeaseManagerV2Test, RefreshModeTryReleaseTwoLeasesAsPerPreference) {
  // Construct mock lease refreshers.
  LeaseManagerV2 lease_manager(move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  // Set up precondition.
  mock_lease_refresher_1_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);
  mock_lease_refresher_2_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);
  mock_lease_refresher_3_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);

  SetUpMocksForLock(mock_leasable_lock_1_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_2_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_3_, this_lease_holder_info_);

  EXPECT_EQ(GetRefresherSetCount(), 3);

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{3, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();

  EXPECT_EQ(GetRefresherSetCount(), 3);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            3);

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{1, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();

  EXPECT_EQ(GetRefresherSetCount(), 5);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease),
            2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            1);

  // Another run does not change mode.
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();
  EXPECT_EQ(GetRefresherSetCount(), 5);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease),
            2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            1);
}

TEST_F(LeaseManagerV2Test,
       RefreshModeCompleteReleaseUponSafeToReleaseInvocation) {
  // Private accessor to set the is_running flag to true without actually
  // running the Lease Manager.
  class LeaseManagerV2PrivateAccessor : public LeaseManagerV2 {
   public:
    explicit LeaseManagerV2PrivateAccessor(
        std::shared_ptr<LeaseRefresherFactoryInterface> lease_refresher_factory)
        : LeaseManagerV2(lease_refresher_factory) {}

    void SetIsRunning() { is_running_ = true; }
  };

  // Construct mock lease refreshers.
  LeaseManagerV2PrivateAccessor lease_manager(
      move(mock_lease_refresher_factory_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_1_, leasable_lock_1_,
                                                 event_sink_1_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_2_, leasable_lock_2_,
                                                 event_sink_2_));
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(lock_id_3_, leasable_lock_3_,
                                                 event_sink_3_));
  EXPECT_SUCCESS(lease_manager.Init());

  lease_manager.SetIsRunning();

  // Set up precondition.
  mock_lease_refresher_1_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);
  mock_lease_refresher_2_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);
  mock_lease_refresher_3_->SetLeaseRefreshMode(
      LeaseRefreshMode::RefreshWithIntentionToHoldLease);

  SetUpMocksForLock(mock_leasable_lock_1_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_2_, this_lease_holder_info_);
  SetUpMocksForLock(mock_leasable_lock_3_, this_lease_holder_info_);

  EXPECT_EQ(GetRefresherSetCount(), 3);

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{3, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();

  EXPECT_EQ(GetRefresherSetCount(), 3);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            3);

  lease_manager.SetLeaseAcquisitionPreference(
      LeaseAcquisitionPreference{1, {}});
  lease_manager.PerformLeaseAcquisitionPreferenceManagement();

  EXPECT_EQ(GetRefresherSetCount(), 5);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease),
            2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            1);

  auto lock_ids = GetLockSetForAMode(
      LeaseRefreshMode::RefreshWithIntentionToReleaseTheHeldLease);
  EXPECT_EQ(lock_ids.size(), 2);

  // 'is_running_' needs to be true for these to go through.
  lease_manager.SafeToReleaseLease(lock_ids[0]);
  lease_manager.SafeToReleaseLease(lock_ids[1]);

  EXPECT_EQ(GetRefresherSetCount(), 7);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithNoIntentionToHoldLease),
            2);
  EXPECT_EQ(GetRefresherSetCountForAMode(
                LeaseRefreshMode::RefreshWithIntentionToHoldLease),
            1);
}
}  // namespace google::scp::core::test
