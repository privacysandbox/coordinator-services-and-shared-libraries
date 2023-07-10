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
#include "initializable_interface.h"

namespace google::scp::core {
/// Represents the get credentials request object.
struct GetCredentialsRequest {};

/// Represents the get credentials response object.
struct GetCredentialsResponse {
  std::shared_ptr<std::string> access_key_id;
  std::shared_ptr<std::string> access_key_secret;
  std::shared_ptr<std::string> security_token;
};

/// Provides cloud credentials functionality.
class CredentialsProviderInterface : public InitializableInterface {
 public:
  virtual ~CredentialsProviderInterface() = default;

  /**
   * @brief Gets the current credentials of the system.
   *
   * @param get_credentials_context The context of the get credentials
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult GetCredentials(
      AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
          get_credentials_context) noexcept = 0;
};
}  // namespace google::scp::core
