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

#include "cc/core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/interface/http_request_response_auth_interceptor_interface.h"

namespace google::scp::core {

class AuthorizationProxy
    : public privacy_sandbox::pbs_common::AuthorizationProxyInterface {
 public:
  struct CacheEntry : public privacy_sandbox::pbs_common::LoadableObject {
    privacy_sandbox::pbs_common::AuthorizedMetadata authorized_metadata;
  };

  AuthorizationProxy(
      const std::string& server_endpoint,
      const std::shared_ptr<
          privacy_sandbox::pbs_common::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<privacy_sandbox::pbs_common::HttpClientInterface>&
          http_client,
      std::unique_ptr<privacy_sandbox::pbs_common::
                          HttpRequestResponseAuthInterceptorInterface>
          http_helper);

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult Authorize(
      privacy_sandbox::pbs_common::AsyncContext<
          privacy_sandbox::pbs_common::AuthorizationProxyRequest,
          privacy_sandbox::pbs_common::AuthorizationProxyResponse>&) noexcept
      override;

 protected:
  /**
   * @brief The handler when performing HttpClient operations.
   *
   * @param authorization_context The authorization context to perform
   * operation on.
   * @param cache_entry_key key of the entry
   * @param http_context
   */
  void HandleAuthorizeResponse(
      privacy_sandbox::pbs_common::AsyncContext<
          privacy_sandbox::pbs_common::AuthorizationProxyRequest,
          privacy_sandbox::pbs_common::AuthorizationProxyResponse>&
          authorization_context,
      std::string& cache_entry_key,
      privacy_sandbox::pbs_common::AsyncContext<
          privacy_sandbox::pbs_common::HttpRequest,
          privacy_sandbox::pbs_common::HttpResponse>& http_context);

  /// The authorization token cache.
  privacy_sandbox::pbs_common::AutoExpiryConcurrentMap<
      std::string, std::shared_ptr<CacheEntry>>
      cache_;

  /// The remote authorization end point URI
  /// Ex: http://localhost:65534/endpoint
  std::shared_ptr<std::string> server_endpoint_uri_;

  /// The host portion of server_endpoint_uri_.
  std::string host_;

  /// The http client to send request to the remote authorizer.
  const std::shared_ptr<privacy_sandbox::pbs_common::HttpClientInterface>
      http_client_;

  /// Request Response helper for HTTP
  std::unique_ptr<
      privacy_sandbox::pbs_common::HttpRequestResponseAuthInterceptorInterface>
      http_helper_;
};
}  // namespace google::scp::core
