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

#include <list>
#include <memory>

#include "core/interface/transaction_manager_interface.h"

namespace google::scp::core::transaction_manager::mock {
class MockTransactionManagerGMock : public TransactionManagerInterface {
 public:
  MOCK_METHOD(ExecutionResult, Init, (), (override, noexcept));
  MOCK_METHOD(ExecutionResult, Run, (), (override, noexcept));
  MOCK_METHOD(ExecutionResult, Stop, (), (override, noexcept));

  MOCK_METHOD(ExecutionResult, Execute,
              ((AsyncContext<TransactionRequest, TransactionResponse>&)),
              (noexcept, override));

  MOCK_METHOD(
      ExecutionResult, ExecutePhase,
      ((AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&)),
      (noexcept, override));

  MOCK_METHOD(ExecutionResult, Checkpoint,
              ((std::shared_ptr<std::list<CheckpointLog>>&)),
              (noexcept, override));

  MOCK_METHOD(ExecutionResult, GetTransactionStatus,
              ((AsyncContext<GetTransactionStatusRequest,
                             GetTransactionStatusResponse>&)),
              (noexcept, override));

  MOCK_METHOD(ExecutionResult, GetTransactionManagerStatus,
              (const GetTransactionManagerStatusRequest&,
               GetTransactionManagerStatusResponse&),
              (noexcept, override));
};
}  // namespace google::scp::core::transaction_manager::mock
