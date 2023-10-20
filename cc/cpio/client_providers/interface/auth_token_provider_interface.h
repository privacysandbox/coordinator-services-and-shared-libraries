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
#include "core/interface/service_interface.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
/// Represents the get session token request object.
struct GetSessionTokenRequest {};

/// Represents the get session token response object.
struct GetSessionTokenResponse {
  std::shared_ptr<std::string> session_token;

  // Time duration in seconds of which token is valid
  std::chrono::seconds token_lifetime_in_seconds;
};

/// Represents the get session token request object for target audience.
struct GetSessionTokenForTargetAudienceRequest {
  std::shared_ptr<std::string> token_target_audience_uri;
};

/**
 * @brief Provides cloud instance authorizer functionality. This class only
 * works on a Cloud virtual machine.
 *
 * For AWS, IMDSv2 is used to fetch the session token. For more information, see
 * https://aws.amazon.com/blogs/security/defense-in-depth-open-firewalls-reverse-proxies-ssrf-vulnerabilities-ec2-instance-metadata-service/
 *
 * For GCP, Instance Metadata Service is used to fetch the OAuth 2.0 access
 * token. For more information, see
 * https://developers.google.com/identity/protocols/oauth2
 */
class AuthTokenProviderInterface : public core::ServiceInterface {
 public:
  virtual ~AuthTokenProviderInterface() = default;

  /**
   * @brief Gets a session token from instance metadata service.
   *
   * @param get_token_context The context of the get session token.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult GetSessionToken(
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context) noexcept = 0;

  /**
   * @brief Gets a session token for target audience.
   *
   * @param get_token_context The context of the get session token.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult GetSessionTokenForTargetAudience(
      core::AsyncContext<GetSessionTokenForTargetAudienceRequest,
                         GetSessionTokenResponse>&
          get_token_context) noexcept = 0;
};

class AuthTokenProviderFactory {
 public:
  /**
   * @brief Factory to create AuthTokenProvider.
   *
   * @return std::shared_ptr<AuthTokenProviderInterface> created
   * AuthTokenProvider.
   */
  static std::shared_ptr<AuthTokenProviderInterface> Create(
      const std::shared_ptr<core::HttpClientInterface>& http1_client);
};
}  // namespace google::scp::cpio::client_providers
