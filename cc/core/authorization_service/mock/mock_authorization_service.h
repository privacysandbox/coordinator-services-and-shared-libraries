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

#include <functional>
#include <memory>
#include <string>

#include "core/interface/authorization_service_interface.h"

namespace google::scp::core::authorization_service::mock {

class MockAuthorizationService : public AuthorizationServiceInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Authorize(
      AsyncContext<AuthorizationRequest, AuthorizationResponse>&
          authorization_context) noexcept override {
    if (authorize_mock) {
      return authorize_mock(authorization_context);
    }

    authorization_context.response = std::make_shared<AuthorizationResponse>();
    authorization_context.response->authorized_domain =
        authorization_context.request->claimed_identity;
    authorization_context.result = SuccessExecutionResult();

    authorization_context.Finish();
    return SuccessExecutionResult();
  }

  std::function<ExecutionResult(
      AsyncContext<AuthorizationRequest, AuthorizationResponse>&)>
      authorize_mock;
};
}  // namespace google::scp::core::authorization_service::mock
