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

#include <atomic>
#include <functional>
#include <memory>

#include "core/interface/async_executor_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/transaction_manager/interface/transaction_engine_interface.h"

namespace google::scp::core::transaction_manager::mock {
class MockRemoteTransactionManager
    : public core::RemoteTransactionManagerInterface {
 public:
  MockRemoteTransactionManager() {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  ExecutionResult GetTransactionStatus(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override {
    if (get_transaction_status_mock) {
      return get_transaction_status_mock(get_transaction_status_context);
    }
    if (transaction_engine.lock()) {
      return transaction_engine.lock()->GetTransactionStatus(
          get_transaction_status_context);
    }
    return core::SuccessExecutionResult();
  }

  ExecutionResult ExecutePhase(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_phase_context) noexcept override {
    if (execute_phase_mock) {
      return execute_phase_mock(transaction_phase_context);
    }
    if (transaction_engine.lock()) {
      return transaction_engine.lock()->ExecutePhase(transaction_phase_context);
    }
    return core::SuccessExecutionResult();
  }

  // Use weak_ptr here to avoid cycle dependencies for shared_ptr.
  std::weak_ptr<TransactionEngineInterface> transaction_engine;
  std::function<ExecutionResult(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&)>
      get_transaction_status_mock;
  std::function<ExecutionResult(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&)>
      execute_phase_mock;
};
}  // namespace google::scp::core::transaction_manager::mock
