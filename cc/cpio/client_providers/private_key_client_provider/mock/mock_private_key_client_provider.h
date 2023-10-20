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

#include "cpio/client_providers/interface/private_key_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockPrivateKeyClientProvider : public PrivateKeyClientProviderInterface {
 public:
  MockPrivateKeyClientProvider() {
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
  }

  MOCK_METHOD(core::ExecutionResult, Init, (), (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, Run, (), (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (override, noexcept));

  MOCK_METHOD(
      core::ExecutionResult, ListPrivateKeys,
      ((core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&)),
      (override, noexcept));
};
}  // namespace google::scp::cpio::client_providers::mock
