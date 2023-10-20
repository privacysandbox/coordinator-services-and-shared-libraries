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

#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/authorization_service_interface.h"
#include "core/interface/http_types.h"

namespace google::scp::core {
/**
 * @brief AwsAuthorizer is an implementation of AuthorizationServiceInterface
 * that provides authorization on AWS.
 *
 * Note that this class is for authenticating/authorizing clients calling into
 * customized service. We expect the client to call the server with a specific
 * header containing a token. The token is a base64 encoded json string, which
 * contains useful parts to compose a SigV4 authorization header. The authN or
 * authZ is then made from the server to a service running on AWS API-Gateway.
 */
class AwsAuthorizer : public AuthorizationServiceInterface {
 protected:
  struct AwsAuthorizationTokenCacheEntry : public LoadableObject {
    std::shared_ptr<std::string> authorized_domain;
  };

 public:
  AwsAuthorizer(const std::string& server_endpoint,
                const std::string& aws_region,
                const std::shared_ptr<AsyncExecutorInterface>& async_executor,
                const std::shared_ptr<HttpClientInterface>& http_client);

  ExecutionResult Init() noexcept override;
  ExecutionResult Run() noexcept override;
  ExecutionResult Stop() noexcept override;
  /**
   * @brief Perform authorization with given \a authorization_context. This call
   * returns after sanity checks and work are done in async.
   *
   * @param authorization_context The authorization context to perform operation
   * on
   * @return ExecutionResult
   */
  ExecutionResult Authorize(
      AsyncContext<AuthorizationRequest, AuthorizationResponse>&
          authorization_context) noexcept override;

 protected:
  /// The authorization token cache.
  std::unique_ptr<common::AutoExpiryConcurrentMap<
      std::string, std::shared_ptr<AwsAuthorizationTokenCacheEntry>>>
      authorization_tokens_;

  /**
   * @brief Is called once any cached tokens are expired and need to be either
   * refreshed or deleted.
   *
   * @param cache_entry_key The claimed identity plus token that is being
   * expired.
   * @param cached_auth_token_entry The entry in the map.
   * @param should_delete_entry Callback to indicate deleting from the map.
   */
  virtual void OnBeforeGarbageCollection(
      std::string& cache_entry_key,
      std::shared_ptr<AwsAuthorizationTokenCacheEntry>& cached_auth_token_entry,
      std::function<void(bool)> should_delete_entry) noexcept;

  /**
   * @brief The handler when performing HttpClient operations.
   *
   * @param authorization_context The authorization context to perform operation
   * on.
   * @param token The actual token passed by the user.
   * @param http_context
   */
  void HandleHttpResponse(
      AsyncContext<AuthorizationRequest, AuthorizationResponse>&
          authorization_context,
      std::string& token,
      AsyncContext<HttpRequest, HttpResponse>& http_context);

 private:
  /// The authorization end point of the AWS.
  std::shared_ptr<std::string> server_endpoint_;
  /// The AWS Region the app is running in.
  const std::string aws_region_;
  /// The http client to send request to the authorizer.
  const std::shared_ptr<HttpClientInterface> http_client_;
};
}  // namespace google::scp::core
