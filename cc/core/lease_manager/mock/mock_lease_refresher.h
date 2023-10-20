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

#pragma once

#include <gmock/gmock.h>

#include <memory>
#include <optional>
#include <string>

#include "core/interface/lease_manager_interface.h"

namespace google::scp::core::lease_manager::mock {
class MockLeaseRefreshLivenessCheck
    : public testing::NiceMock<LeaseRefreshLivenessCheckInterface> {
 public:
  MOCK_METHOD(std::chrono::nanoseconds, GetLastLeaseRefreshTimestamp, (),
              (const, noexcept));
};

class MockLeaseRefresher : public testing::NiceMock<LeaseRefresherInterface> {
 public:
  MockLeaseRefresher() {
    ON_CALL(*this, SetLeaseRefreshMode)
        .WillByDefault([this](LeaseRefreshMode mode) {
          mode_ = mode;
          refresh_counter_++;
          return SuccessExecutionResult();
        });
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(SuccessExecutionResult()));
    ON_CALL(*this, GetLeaseRefreshMode).WillByDefault([this]() {
      return mode_;
    });
    ON_CALL(*this, PerformLeaseRefresh)
        .WillByDefault(testing::Return(SuccessExecutionResult()));
  }

  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));

  MOCK_METHOD(ExecutionResult, Run, (), (noexcept, override));

  MOCK_METHOD(ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(LeaseRefreshMode, GetLeaseRefreshMode, (), (const, noexcept));

  MOCK_METHOD(ExecutionResult, SetLeaseRefreshMode, (LeaseRefreshMode),
              (noexcept));

  MOCK_METHOD(ExecutionResult, PerformLeaseRefresh, (), (noexcept));

  LeaseRefreshMode mode_ = LeaseRefreshMode::RefreshWithNoIntentionToHoldLease;

  std::atomic<size_t> refresh_counter_ = 0;
};

}  // namespace google::scp::core::lease_manager::mock
