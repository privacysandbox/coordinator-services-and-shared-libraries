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

namespace privacy_sandbox::pbs {
class LocalAuthorizationProxy : public pbs_common::AuthorizationProxyInterface {
 public:
  pbs_common::ExecutionResult Init() noexcept override {
    return pbs_common::SuccessExecutionResult();
  }

  pbs_common::ExecutionResult Run() noexcept override {
    return pbs_common::SuccessExecutionResult();
  }

  pbs_common::ExecutionResult Stop() noexcept override {
    return pbs_common::SuccessExecutionResult();
  }

  pbs_common::ExecutionResult Authorize(
      pbs_common::AsyncContext<pbs_common::AuthorizationProxyRequest,
                               pbs_common::AuthorizationProxyResponse>&
          context) noexcept override {
    context.result = pbs_common::SuccessExecutionResult();
    context.response =
        std::make_shared<pbs_common::AuthorizationProxyResponse>();
    context.response->authorized_metadata.authorized_domain =
        std::make_shared<std::string>(
            context.request->authorization_metadata.claimed_identity);
    context.Finish();
    return pbs_common::SuccessExecutionResult();
  }
};
}  // namespace privacy_sandbox::pbs
