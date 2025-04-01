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

#include <memory>
#include <string>

#include "cc/core/interface/authorization_proxy_interface.h"

namespace google::scp::pbs {
class LocalAuthorizationProxy
    : public privacy_sandbox::pbs_common::AuthorizationProxyInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Authorize(
      privacy_sandbox::pbs_common::AsyncContext<
          privacy_sandbox::pbs_common::AuthorizationProxyRequest,
          privacy_sandbox::pbs_common::AuthorizationProxyResponse>&
          context) noexcept override {
    context.result = core::SuccessExecutionResult();
    context.response = std::make_shared<
        privacy_sandbox::pbs_common::AuthorizationProxyResponse>();
    context.response->authorized_metadata.authorized_domain =
        std::make_shared<std::string>(
            context.request->authorization_metadata.claimed_identity);
    context.Finish();
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs
