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

#include "core/interface/async_context.h"
#include "core/interface/http_client_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {

class MockAuthTokenProvider : public AuthTokenProviderInterface {
 public:
  MOCK_METHOD(core::ExecutionResult, Init, (), (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, Run, (), (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (override, noexcept));

  MOCK_METHOD(
      core::ExecutionResult, GetSessionToken,
      ((core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&)),
      (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, GetSessionTokenForTargetAudience,
              ((core::AsyncContext<GetSessionTokenForTargetAudienceRequest,
                                   GetSessionTokenResponse>&)),
              (override, noexcept));
};

}  // namespace google::scp::cpio::client_providers::mock
