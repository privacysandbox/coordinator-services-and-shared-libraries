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

#include "cpio/client_providers/interface/public_key_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockPublicKeyClientProvider : public PublicKeyClientProviderInterface {
 public:
  MockPublicKeyClientProvider() {
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

  MOCK_METHOD(core::ExecutionResult, ListPublicKeys,
              ((core::AsyncContext<
                  cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
                  cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>&)),
              (override, noexcept));
};
}  // namespace google::scp::cpio::client_providers::mock
