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

#include "core/common/time_provider/src/time_provider.h"
#include "core/lease_manager/mock/mock_lease_refresher.h"
#include "core/lease_manager/src/v2/error_codes.h"
#include "core/lease_manager/src/v2/lease_refresh_liveness_enforcer.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::common::TimeProvider;
using google::scp::core::lease_manager::mock::MockLeaseRefreshLivenessCheck;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using ::testing::Invoke;
using ::testing::Return;

namespace google::scp::core::test {
class LeaseRefreshLivenessEnforcerTest : public ::testing::Test {
 protected:
  LeaseRefreshLivenessEnforcerTest() {
    mock_refreshers_.push_back(
        std::make_shared<MockLeaseRefreshLivenessCheck>());
    refreshers_.push_back(mock_refreshers_.back());
    ON_CALL(*mock_refreshers_.back(), GetLastLeaseRefreshTimestamp())
        .WillByDefault(TimeProvider::GetSteadyTimestampInNanoseconds);

    mock_refreshers_.push_back(
        std::make_shared<MockLeaseRefreshLivenessCheck>());
    refreshers_.push_back(mock_refreshers_.back());
    ON_CALL(*mock_refreshers_.back(), GetLastLeaseRefreshTimestamp())
        .WillByDefault(TimeProvider::GetSteadyTimestampInNanoseconds);

    mock_refreshers_.push_back(
        std::make_shared<MockLeaseRefreshLivenessCheck>());
    refreshers_.push_back(mock_refreshers_.back());
    ON_CALL(*mock_refreshers_.back(), GetLastLeaseRefreshTimestamp())
        .WillByDefault(TimeProvider::GetSteadyTimestampInNanoseconds);
  }

  std::vector<std::shared_ptr<MockLeaseRefreshLivenessCheck>> mock_refreshers_;
  std::vector<std::shared_ptr<LeaseRefreshLivenessCheckInterface>> refreshers_;
  std::chrono::milliseconds lease_duration_in_milliseconds_ =
      std::chrono::seconds(5);
};

TEST_F(LeaseRefreshLivenessEnforcerTest, InitRunStop) {
  LeaseRefreshLivenessEnforcer enforcer(refreshers_,
                                        lease_duration_in_milliseconds_);
  EXPECT_SUCCESS(enforcer.Init());
  EXPECT_SUCCESS(enforcer.Run());
  EXPECT_SUCCESS(enforcer.Stop());
}

TEST_F(LeaseRefreshLivenessEnforcerTest,
       InitWithLeaseDurationLessThanEnforcementIntervalFails) {
  LeaseRefreshLivenessEnforcer enforcer(refreshers_,
                                        std::chrono::milliseconds(1));
  EXPECT_THAT(enforcer.Init(),
              ResultIs(FailureExecutionResult(
                  errors::SC_LEASE_LIVENESS_ENFORCER_INSUFFICIENT_PERIOD)));
}

TEST_F(LeaseRefreshLivenessEnforcerTest, EnforcesPeriodically) {
  LeaseRefreshLivenessEnforcer enforcer(refreshers_,
                                        lease_duration_in_milliseconds_);
  std::atomic<size_t> is_completed_count(0);
  for (auto& refresher : mock_refreshers_) {
    EXPECT_CALL(*refresher, GetLastLeaseRefreshTimestamp())
        .WillOnce(
            Return(Invoke(TimeProvider::GetSteadyTimestampInNanoseconds())))
        .WillOnce(
            Return(Invoke(TimeProvider::GetSteadyTimestampInNanoseconds())))
        .WillRepeatedly([&is_completed_count]() {
          is_completed_count++;
          return TimeProvider::GetSteadyTimestampInNanoseconds();
        });
  }
  EXPECT_SUCCESS(enforcer.Run());
  WaitUntil([&is_completed_count, this]() {
    return is_completed_count == mock_refreshers_.size();
  });
  EXPECT_SUCCESS(enforcer.Stop());
}

TEST_F(LeaseRefreshLivenessEnforcerTest,
       EnforcesPeriodicallyAndTakesCorrectiveAction) {
  std::atomic<bool> corrective_action_invoked(false);
  LeaseRefreshLivenessEnforcer enforcer(
      refreshers_, lease_duration_in_milliseconds_,
      [&corrective_action_invoked]() { corrective_action_invoked = true; });
  for (auto& refresher : mock_refreshers_) {
    EXPECT_CALL(*refresher, GetLastLeaseRefreshTimestamp())
        .WillOnce(
            Return(Invoke(TimeProvider::GetSteadyTimestampInNanoseconds())))
        .WillRepeatedly(Return(std::chrono::seconds(0)));
  }
  EXPECT_SUCCESS(enforcer.Run());
  WaitUntil(
      [&corrective_action_invoked]() {
        return corrective_action_invoked.load();
      },
      std::chrono::seconds(10));
  EXPECT_SUCCESS(enforcer.Stop());
}
}  // namespace google::scp::core::test
