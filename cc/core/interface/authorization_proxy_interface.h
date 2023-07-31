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
#include "http_types.h"
#include "service_interface.h"

namespace google::scp::core {
typedef std::string AuthorizationToken;
typedef std::string AuthorizedDomain;
typedef std::string ClaimedIdentity;

/**
 * @brief Structure to represent the metada of the
 *
 */
struct AuthorizationMetadata {
  /// Parameters related to authorization request
  ClaimedIdentity claimed_identity;
  AuthorizationToken authorization_token;

  ~AuthorizationMetadata() = default;

  /**
   * @brief Checks the validity of the object
   *
   * @return true
   * @return false
   */
  virtual bool IsValid() const {
    if (claimed_identity.empty() || authorization_token.empty()) {
      return false;
    }
    return true;
  }

  /**
   * @brief Get a unique key for the object
   *
   * @return std::string
   */
  std::string GetKey() const { return claimed_identity + authorization_token; }
};

/**
 * @brief Structure to represent the metadata of an authorized request
 *
 */
struct AuthorizedMetadata {
  // Shared pointer for string re-use.
  std::shared_ptr<AuthorizedDomain> authorized_domain;
  // Other fields as needed (all of them need to be shared_ptrs)

  ~AuthorizedMetadata() = default;
};

/**
 * @brief Request object for Authorization Proxy
 * This contains the authorization token supplied by user, which will be
 * validated by the remote service
 */
struct AuthorizationProxyRequest {
  /// Authorization metadata.
  AuthorizationMetadata authorization_metadata;
};

/**
 * @brief Response object which holds the authorized metadata
 */
struct AuthorizationProxyResponse {
  /// Authorized metadata
  AuthorizedMetadata authorized_metadata;
};

class AuthorizationProxyInterface : public ServiceInterface {
 public:
  virtual ~AuthorizationProxyInterface() = default;

  /**
   * @brief Authorizes the request
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult Authorize(
      AsyncContext<AuthorizationProxyRequest,
                   AuthorizationProxyResponse>&) noexcept = 0;
};
}  // namespace google::scp::core
