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
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc PrivateKeyFetcherProviderInterface
 */
class PrivateKeyFetcherProvider : public PrivateKeyFetcherProviderInterface {
 public:
  virtual ~PrivateKeyFetcherProvider() = default;

  explicit PrivateKeyFetcherProvider(
      const std::shared_ptr<core::HttpClientInterface>& http_client)
      : http_client_(http_client) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult FetchPrivateKey(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          private_key_fetching_context) noexcept override;

 protected:
  /**
   * @brief Sign Http request with credentials.
   *
   * @param sign_http_request_context execution context.
   * @return core::ExecutionResult execution result.
   */
  virtual core::ExecutionResult SignHttpRequest(
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context) noexcept = 0;

  /**
   * @brief Triggered to fetch private key when http request is signed.
   *
   * @param private_key_fetching_context context to fetch private key.
   * @param sign_http_request_context context to sign http request.
   * @return core::ExecutionResult execution result.
   */
  void SignHttpRequestCallback(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          private_key_fetching_context,
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context) noexcept;

  /**
   * @brief Triggered to parse private key when private key payload is fetched.
   *
   * @param private_key_fetching_context context to fetch private key.
   * @param http_client_context context to perform http request.
   * @return core::ExecutionResult execution result.
   */
  void PrivateKeyFetchingCallback(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          private_key_fetching_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  /// HttpClient for issuing HTTP actions.
  std::shared_ptr<core::HttpClientInterface> http_client_;
};
}  // namespace google::scp::cpio::client_providers
