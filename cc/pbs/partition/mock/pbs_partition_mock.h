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

#pragma once

#include <gmock/gmock.h>

#include <functional>
#include <memory>
#include <string>

#include "pbs/interface/pbs_partition_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::partition::mock {

class MockPBSPartition : public testing::NiceMock<PBSPartitionInterface> {
 public:
  MOCK_METHOD(core::ExecutionResult, Init, (), (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, Load, (), (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, Unload, (), (override, noexcept));

  MOCK_METHOD(core::PartitionLoadUnloadState, GetPartitionState, (),
              (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, ExecuteRequest,
              ((core::AsyncContext<core::TransactionPhaseRequest,
                                   core::TransactionPhaseResponse> &
                context)),
              (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, ExecuteRequest,
              ((core::AsyncContext<core::TransactionRequest,
                                   core::TransactionResponse> &
                context)),
              (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, GetTransactionStatus,
              ((core::AsyncContext<core::GetTransactionStatusRequest,
                                   core::GetTransactionStatusResponse> &
                context)),
              (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, GetTransactionManagerStatus,
              (const core::GetTransactionManagerStatusRequest& request,
               core::GetTransactionManagerStatusResponse& response),
              (override, noexcept));

  std::atomic<core::PartitionLoadUnloadState> partition_state_ =
      core::PartitionLoadUnloadState::Created;
};

}  // namespace google::scp::pbs::partition::mock
