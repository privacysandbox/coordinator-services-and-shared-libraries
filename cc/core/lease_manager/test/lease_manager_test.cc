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

#include "core/lease_manager/src/lease_manager.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/lease_manager/mock/mock_leasable_lock.h"
#include "core/lease_manager/mock/mock_lease_manager_with_overrides.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using std::abort;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_lock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

using google::scp::core::LeaseInfo;
using google::scp::core::lease_manager::mock::MockLeasableLock;
using google::scp::core::lease_manager::mock::MockLeaseManagerWithOverrides;

namespace google::scp::core::test {

TEST(LeaseManagerTest, InitStartRunStop) {
  MockLeaseManagerWithOverrides lease_manager(
      100 /* lease enforcer periodicity */,
      1000 /* lease obtainer max running time */);
  auto leasable_lock = make_shared<MockLeasableLock>();
  leasable_lock->SetLeaseDurationInMilliseconds(500);

  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_EQ(lease_manager.ManageLeaseOnLock(
                leasable_lock, [](LeaseTransitionType lease_transition_type,
                                  optional<LeaseInfo> lease_owner) {}),
            SuccessExecutionResult());
  EXPECT_SUCCESS(lease_manager.Run());
  // Run() on an already running lease manager should fail.
  EXPECT_NE(lease_manager.Run(), SuccessExecutionResult());

  // ManageLeaseOnLock() while LeaseManager is running should fail.
  EXPECT_NE(lease_manager.ManageLeaseOnLock(
                leasable_lock, [](LeaseTransitionType, optional<LeaseInfo>) {}),
            SuccessExecutionResult());

  EXPECT_SUCCESS(lease_manager.Stop());
  // Stop() on an already running lease manager should fail.
  EXPECT_NE(lease_manager.Stop(), SuccessExecutionResult());
}

TEST(LeaseManagerTest, StartsAndStopsThreads) {
  MockLeaseManagerWithOverrides lease_manager(
      100 /* lease enforcer periodicity */,
      1000 /* lease obtainer max running time */);
  auto leasable_lock = make_shared<MockLeasableLock>();
  leasable_lock->SetLeaseDurationInMilliseconds(500);

  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_EQ(lease_manager.ManageLeaseOnLock(
                leasable_lock, [](LeaseTransitionType lease_transition_type,
                                  optional<LeaseInfo> lease_owner) {}),
            SuccessExecutionResult());
  EXPECT_SUCCESS(lease_manager.Run());
  EXPECT_TRUE(lease_manager.IsLeaseEnforcerThreadStarted());
  EXPECT_TRUE(lease_manager.IsLeaseObtainerThreadStarted());

  EXPECT_SUCCESS(lease_manager.Stop());
  EXPECT_FALSE(lease_manager.IsLeaseEnforcerThreadStarted());
  EXPECT_FALSE(lease_manager.IsLeaseObtainerThreadStarted());
}

TEST(LeaseManagerTest, AcquiresAndRenewsLease) {
  atomic<bool> process_terminated(false);
  atomic<int> callback_counter(0);

  LeaseInfo lease_acquirer_info;
  lease_acquirer_info.lease_acquirer_id = "123";
  lease_acquirer_info.service_endpoint_address = "10.1.1.1";

  MockLeaseManagerWithOverrides lease_manager(
      100 /* lease enforcer periodicity */,
      1000 /* lease obtainer max running time */);
  lease_manager.SetTerminateProcessFunction(
      [&process_terminated]() { process_terminated = true; });

  auto leasable_lock = make_shared<MockLeasableLock>();
  leasable_lock->SetLeaseDurationInMilliseconds(500);
  leasable_lock->SetLeaseOwnerInfo(lease_acquirer_info);
  leasable_lock->AllowLeaseAcquire();
  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_EQ(
      lease_manager.ManageLeaseOnLock(
          leasable_lock,
          [&](LeaseTransitionType lease_transition, optional<LeaseInfo> owner) {
            EXPECT_TRUE(owner.has_value());
            if (callback_counter == 0) {
              EXPECT_EQ(lease_transition, LeaseTransitionType::kAcquired);
              EXPECT_EQ(owner->service_endpoint_address,
                        lease_acquirer_info.service_endpoint_address);
            } else {
              EXPECT_EQ(lease_transition, LeaseTransitionType::kRenewed);
              EXPECT_EQ(owner->service_endpoint_address,
                        lease_acquirer_info.service_endpoint_address);
            }
            callback_counter++;
          }),
      SuccessExecutionResult());

  EXPECT_SUCCESS(lease_manager.Run());
  while (callback_counter <= 1) {
    sleep_for(milliseconds(10));
  }
  EXPECT_SUCCESS(lease_manager.Stop());
  EXPECT_FALSE(process_terminated);
}

TEST(LeaseManagerTest, LosesAndReacquiresLease) {
  atomic<bool> process_terminated(false);
  atomic<int> callback_counter = {0};

  LeaseInfo lease_acquirer_info;
  lease_acquirer_info.lease_acquirer_id = "123";
  lease_acquirer_info.service_endpoint_address = "10.1.1.1";

  MockLeaseManagerWithOverrides lease_manager(
      100 /* lease enforcer periodicity */,
      1000 /* lease obtainer max running time */);
  lease_manager.SetTerminateProcessFunction(
      [&process_terminated]() { process_terminated = true; });

  auto leasable_lock = make_shared<MockLeasableLock>();
  leasable_lock->SetLeaseDurationInMilliseconds(500);
  leasable_lock->SetLeaseOwnerInfo(lease_acquirer_info);
  leasable_lock->AllowLeaseAcquire();
  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_EQ(
      lease_manager.ManageLeaseOnLock(
          leasable_lock,
          [&](LeaseTransitionType lease_transition, optional<LeaseInfo> owner) {
            if (callback_counter == 0) {
              EXPECT_TRUE(owner.has_value());
              EXPECT_EQ(lease_transition, LeaseTransitionType::kAcquired);
              EXPECT_EQ(owner->service_endpoint_address,
                        lease_acquirer_info.service_endpoint_address);
            } else if (callback_counter == 1) {
              EXPECT_TRUE(owner.has_value());
              EXPECT_EQ(lease_transition, LeaseTransitionType::kRenewed);
              EXPECT_EQ(owner->service_endpoint_address,
                        lease_acquirer_info.service_endpoint_address);
              leasable_lock->DisallowLeaseAcquire();
            } else if (callback_counter == 2) {
              EXPECT_FALSE(owner.has_value());
              EXPECT_EQ(lease_transition, LeaseTransitionType::kLost);
            } else if (callback_counter == 3) {
              EXPECT_FALSE(owner.has_value());
              EXPECT_EQ(lease_transition, LeaseTransitionType::kNotAcquired);
              leasable_lock->AllowLeaseAcquire();
            } else {
              EXPECT_TRUE(owner.has_value());
              EXPECT_EQ(lease_transition, LeaseTransitionType::kAcquired);
              EXPECT_EQ(owner->service_endpoint_address,
                        lease_acquirer_info.service_endpoint_address);
            }
            callback_counter++;
          }),
      SuccessExecutionResult());

  EXPECT_SUCCESS(lease_manager.Run());
  while (callback_counter <= 4) {
    sleep_for(milliseconds(10));
  }
  EXPECT_SUCCESS(lease_manager.Stop());
  EXPECT_FALSE(process_terminated);
}

TEST(LeaseManagerTest,
     ProcessTerminatesIfLeaseTransitionCallbackInvocationTakesLong) {
  atomic<bool> process_terminated(false);

  uint64_t lease_enforcer_frequency_in_milliseconds = 100;
  uint64_t lease_obtain_max_time_threshold_in_milliseconds = 500;
  MockLeaseManagerWithOverrides lease_manager(
      lease_enforcer_frequency_in_milliseconds,
      lease_obtain_max_time_threshold_in_milliseconds);
  lease_manager.SetTerminateProcessFunction(
      [&process_terminated]() { process_terminated = true; });

  auto leasable_lock = make_shared<MockLeasableLock>();
  leasable_lock->AllowLeaseAcquire();
  leasable_lock->SetLeaseDurationInMilliseconds(500);
  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_EQ(
      lease_manager.ManageLeaseOnLock(
          leasable_lock,
          [&](LeaseTransitionType lease_transition, optional<LeaseInfo> owner) {
            sleep_for(milliseconds(
                lease_obtain_max_time_threshold_in_milliseconds * 2));
          }),
      SuccessExecutionResult());

  EXPECT_SUCCESS(lease_manager.Run());
  while (!process_terminated) {
    sleep_for(milliseconds(10));
  }
  EXPECT_SUCCESS(lease_manager.Stop());
  EXPECT_TRUE(process_terminated);
}

TEST(LeaseManagerTest, ProcessTerminatesIfLeaseAcquireOnLockTakesLong) {
  atomic<bool> process_terminated(false);

  uint64_t lease_enforcer_frequency_in_milliseconds = 100;
  uint64_t lease_obtain_max_time_threshold_in_milliseconds = 500;
  MockLeaseManagerWithOverrides lease_manager(
      lease_enforcer_frequency_in_milliseconds,
      lease_obtain_max_time_threshold_in_milliseconds);
  lease_manager.SetTerminateProcessFunction(
      [&process_terminated]() { process_terminated = true; });

  auto leasable_lock = make_shared<MockLeasableLock>();
  leasable_lock->SetLeaseDurationInMilliseconds(500);
  leasable_lock->AllowLeaseAcquire();
  leasable_lock->SetOnBeforeAcquireLease([&]() {
    sleep_for(
        milliseconds(lease_obtain_max_time_threshold_in_milliseconds * 2));
  });
  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_EQ(lease_manager.ManageLeaseOnLock(
                leasable_lock, [&](LeaseTransitionType lease_transition,
                                   optional<LeaseInfo> owner) {}),
            SuccessExecutionResult());

  EXPECT_SUCCESS(lease_manager.Run());
  while (!process_terminated) {
    sleep_for(milliseconds(10));
  }
  EXPECT_SUCCESS(lease_manager.Stop());
  EXPECT_TRUE(process_terminated);
}

}  // namespace google::scp::core::test
