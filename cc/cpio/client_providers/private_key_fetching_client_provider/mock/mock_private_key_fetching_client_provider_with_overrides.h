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

#include "core/interface/async_context.h"
#include "core/interface/http_client_interface.h"
#include "cpio/client_providers/private_key_fetching_client_provider/src/private_key_fetching_client_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockPrivateKeyFetchingClientProviderWithOverrides
    : public PrivateKeyFetchingClientProvider {
 public:
  MockPrivateKeyFetchingClientProviderWithOverrides(
      const std::shared_ptr<core::HttpClientInterface>& http_client)
      : PrivateKeyFetchingClientProvider(http_client) {}

  core::ExecutionResult sign_http_request_result_mock =
      core::SuccessExecutionResult();

 private:
  core::ExecutionResult SignHttpRequest(
      core::AsyncContext<core::HttpRequest, core::HttpRequest>&
          sign_http_request_context,
      const std::shared_ptr<std::string>& region,
      const std::shared_ptr<AccountIdentity>& account_identity) noexcept
      override {
    sign_http_request_context.result = sign_http_request_result_mock;
    sign_http_request_context.response = sign_http_request_context.request;
    sign_http_request_context.Finish();
    return sign_http_request_context.result;
  }
};
}  // namespace google::scp::cpio::client_providers::mock
