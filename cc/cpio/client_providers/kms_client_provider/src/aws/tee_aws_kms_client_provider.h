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
#include "core/interface/credentials_provider_interface.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc KmsClientProviderInterface
 */
class TeeAwsKmsClientProvider : public KmsClientProviderInterface {
 public:
  /**
   * @brief Constructs a new Aws Enclaves Kms Client Provider.
   *
   * @param credential_provider the credential provider.
   */
  explicit TeeAwsKmsClientProvider(
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          credential_provider)
      : credential_provider_(credential_provider) {}

  TeeAwsKmsClientProvider() = delete;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult Decrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context) noexcept override;

 protected:
  /**
   * @brief Callback to pass session credentials for decryption.
   *
   * @param create_kms_context the context of created KMS Client.
   * @param get_session_credentials_contexts the context of fetched session
   * credentials.
   * @return core::ExecutionResult the creation results.
   */
  void GetSessionCredentialsCallbackToDecrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context,
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_session_credentials_context) noexcept;

  virtual core::ExecutionResult DecryptUsingEnclavesKmstoolCli(
      const std::string& command, std::string& plaintext) noexcept;

  /// Credential provider.
  const std::shared_ptr<RoleCredentialsProviderInterface> credential_provider_;
};
}  // namespace google::scp::cpio::client_providers
