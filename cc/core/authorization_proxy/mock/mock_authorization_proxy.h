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

#include <functional>
#include <memory>
#include <string>

#include "core/interface/authorization_proxy_interface.h"

namespace google::scp::core::authorization_proxy::mock {

class MockAuthorizationProxy : public AuthorizationProxyInterface {
 public:
  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));

  MOCK_METHOD(ExecutionResult, Run, (), (noexcept, override));

  MOCK_METHOD(ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(
      ExecutionResult, Authorize,
      ((AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&)),
      (noexcept, override));
};
}  // namespace google::scp::core::authorization_proxy::mock
