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

#include "async_context.h"
#include "service_interface.h"

namespace google::scp::core {
typedef std::string AuthorizationToken;
typedef std::string AuthorizedDomain;

/// Represents the authorization request object.
struct AuthorizationRequest {
  /// The token provided by the caller.
  std::shared_ptr<AuthorizationToken> authorization_token;
  // The claimed identity of the caller
  std::shared_ptr<std::string> claimed_identity;
};

/// Represents the authorization response object.
struct AuthorizationResponse {
  std::shared_ptr<AuthorizedDomain> authorized_domain;
};

/// Provides authorization functionality for Http servers.
class AuthorizationServiceInterface : public ServiceInterface {
 public:
  virtual ~AuthorizationServiceInterface() = default;

  /**
   * @brief Validates the provided token by the caller and return success if the
   * token is valid.
   *
   * @param authorization_context The context of the authorization operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Authorize(
      AsyncContext<AuthorizationRequest, AuthorizationResponse>&
          authorization_context) noexcept = 0;
};
}  // namespace google::scp::core
