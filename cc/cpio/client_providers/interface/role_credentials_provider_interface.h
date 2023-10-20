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
#include "core/interface/async_executor_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
/// Configurations for RoleCredentialProvider.
struct RoleCredentialsProviderOptions {
  virtual ~RoleCredentialsProviderOptions() = default;
};

/// Represents the get credentials request object.
struct GetRoleCredentialsRequest {
  // The identity of the account. In AWS, it would be IAM role. In GCP, it would
  // be the service account.
  std::shared_ptr<AccountIdentity> account_identity;
};

/// Represents the get credentials response object.
struct GetRoleCredentialsResponse {
  std::shared_ptr<std::string> access_key_id;
  std::shared_ptr<std::string> access_key_secret;
  std::shared_ptr<std::string> security_token;
};

/// Provides cloud role credentials functionality.
class RoleCredentialsProviderInterface : public core::ServiceInterface {
 public:
  virtual ~RoleCredentialsProviderInterface() = default;

  /**
   * @brief Gets the role credentials for the given AccountIdentity.
   *
   * @param get_credentials_context The context of the get role credentials
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult GetRoleCredentials(
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_role_credentials_context) noexcept = 0;
};

class RoleCredentialsProviderFactory {
 public:
  /**
   * @brief Factory to create RoleCredentialsProviderInterface.
   *
   * @return std::shared_ptr<RoleCredentialsProviderInterface> created
   * RoleCredentialsProviderInterface.
   */
  static std::shared_ptr<RoleCredentialsProviderInterface> Create(
      const std::shared_ptr<RoleCredentialsProviderOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers
