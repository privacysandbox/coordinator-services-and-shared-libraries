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
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/private_key_fetcher_provider.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc PrivateKeyFetcherProviderInterface
 */
class GcpPrivateKeyFetcherProvider : public PrivateKeyFetcherProvider {
 public:
  /**
   * @brief Constructs a new GCP Private Key Fetching Client Provider object.
   *
   * @param http_client http client to issue http requests.
   * @param auth_token_provider auth token provider.
   * service.
   */
  GcpPrivateKeyFetcherProvider(
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider)
      : PrivateKeyFetcherProvider(http_client),
        auth_token_provider_(auth_token_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult SignHttpRequest(
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context) noexcept override;

 private:
  /**
   * @brief Is called after auth_token_provider GetSessionToken() for session
   * token is completed
   *
   * @param sign_http_request_context the context for sign http request.
   * @param get_session_token the context of get session token.
   */
  void OnGetSessionTokenCallback(
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context,
      core::AsyncContext<GetSessionTokenForTargetAudienceRequest,
                         GetSessionTokenResponse>& get_session_token) noexcept;

  // Auth token provider.
  std::shared_ptr<AuthTokenProviderInterface> auth_token_provider_;
};
}  // namespace google::scp::cpio::client_providers
