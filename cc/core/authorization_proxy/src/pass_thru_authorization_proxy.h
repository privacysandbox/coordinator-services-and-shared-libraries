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

#include "core/interface/authorization_proxy_interface.h"

namespace google::scp::core {

/**
 * @brief Simple synchronous authorizer that completes a context with
 * success.
 */
class PassThruAuthorizationProxy : public AuthorizationProxyInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Authorize(
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
          authorization_context) noexcept override {
    authorization_context.response =
        std::make_shared<AuthorizationProxyResponse>();
    // Return the requested domain as authorized.
    authorization_context.response->authorized_metadata
        .authorized_domain = std::make_shared<std::string>(
        authorization_context.request->authorization_metadata.claimed_identity);
    authorization_context.result = SuccessExecutionResult();
    authorization_context.Finish();
    return SuccessExecutionResult();
  }
};

/**
 * @brief PassThruAuthorizationProxyAsync
 * This is helpful if the user wants to mimic async completion of context.
 */
class PassThruAuthorizationProxyAsync : public AuthorizationProxyInterface {
 public:
  PassThruAuthorizationProxyAsync(
      std::shared_ptr<AsyncExecutorInterface> async_executor)
      : async_executor_(async_executor) {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Authorize(
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
          authorization_context) noexcept override {
    authorization_context.response =
        std::make_shared<AuthorizationProxyResponse>();
    // Return the requested domain as authorized.
    authorization_context.response->authorized_metadata
        .authorized_domain = std::make_shared<std::string>(
        authorization_context.request->authorization_metadata.claimed_identity);
    FinishContext(SuccessExecutionResult(), authorization_context,
                  async_executor_);
    return SuccessExecutionResult();
  }

 private:
  std::shared_ptr<AsyncExecutorInterface> async_executor_;
};
}  // namespace google::scp::core
