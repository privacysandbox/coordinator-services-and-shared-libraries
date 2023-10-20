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

#include "core/interface/transaction_request_router_interface.h"

namespace google::scp::pbs::partition_request_router::mock {
class MockTransactionRequestRouter
    : public core::TransactionRequestRouterInterface {
 public:
  MOCK_METHOD(core::ExecutionResult, Execute,
              ((core::AsyncContext<core::TransactionRequest,
                                   core::TransactionResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Execute,
              ((core::AsyncContext<core::TransactionPhaseRequest,
                                   core::TransactionPhaseResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Execute,
              ((core::AsyncContext<core::GetTransactionStatusRequest,
                                   core::GetTransactionStatusResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Execute,
              (const core::GetTransactionManagerStatusRequest&,
               core::GetTransactionManagerStatusResponse&),
              (noexcept, override));
};
}  // namespace google::scp::pbs::partition_request_router::mock
