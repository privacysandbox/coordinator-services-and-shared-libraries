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

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/kms/KMSClient.h>
#include <tink/aead.h>

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc KmsClientProviderInterface
 */
class NonteeAwsKmsClientProvider : public KmsClientProviderInterface {
 public:
  /**
   * @brief Constructs a new Aws Kms Client Provider.
   *
   * @param assume_role_arn the AssumRole ARN.
   * @param credentials_provider the credential provider.
   * @param region the AWS region.
   * @param async_executor the thread pool for batch recording.
   * @param io_async_executor the thread pool for batch recording.
   */
  explicit NonteeAwsKmsClientProvider(
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor)
      : role_credentials_provider_(role_credentials_provider),
        io_async_executor_(io_async_executor) {}

  NonteeAwsKmsClientProvider() = delete;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult Decrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context) noexcept override;

 protected:
  /**
   * @brief Callback to pass Aead for decryption.
   *
   * @param decrypt_context the context of decrpytion.
   * @param get_aead_context the context of fetched AEAD.
   * @return core::ExecutionResult the creation results.
   */
  core::ExecutionResult GetAeadCallbackToDecrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context,
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         crypto::tink::Aead>& get_aead_context) noexcept;

  /**
   * @brief Creates the Client Config object.
   *
   * @param region the region of the client.
   * @return std::shared_ptr<Aws::Client::ClientConfiguration> client
   * configuration.
   */
  virtual std::shared_ptr<Aws::Client::ClientConfiguration>
  CreateClientConfiguration(const std::string& region) noexcept;

  /**
   * @brief Creates a KMS Client object.
   *
   * @param create_kms_context the context of created KMS Client.
   * @return core::ExecutionResult the creation results.
   */
  core::ExecutionResult CreateKmsClient(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         Aws::KMS::KMSClient>& create_kms_context) noexcept;

  /**
   * @brief Callback to pass session credentials to create KMS Client.
   *
   * @param create_kms_context the context of created KMS Client.
   * @param get_session_credentials_contexts the context of fetched session
   * credentials.
   * @return core::ExecutionResult the creation results.
   */
  void GetSessionCredentialsCallbackToCreateKms(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         Aws::KMS::KMSClient>& create_kms_context,
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_role_credentials_contexts) noexcept;

  /**
   * @brief Fetches session credentials.
   *
   * @param get_session_credentials_context the context of fetched session
   * credentials.
   * @return core::ExecutionResult the creation results.
   */
  core::ExecutionResult GetRoleCredentials(
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_role_credentials_context) noexcept;

  /**
   * @brief Fetches KMS Aead.
   *
   * @param get_aead_context the context of KMS Aead.
   * @return core::ExecutionResult the creation results.
   */
  core::ExecutionResult GetAead(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         crypto::tink::Aead>& get_aead_context) noexcept;

  /**
   * @brief Callback to pass KMS Client to create Aead.
   *
   * @param get_aead_context the context of KMS Aead.
   * @param create_kms_context the context of created KMS Client.
   * @return core::ExecutionResult the creation results.
   */
  void CreateKmsCallbackToCreateAead(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         crypto::tink::Aead>& get_aead_context,
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         Aws::KMS::KMSClient>& create_kms_context) noexcept;

  /**
   * @brief Gets a KMS Client object.
   * @return Aws::KMS::KMSClient the KMS Client.
   */
  virtual std::shared_ptr<Aws::KMS::KMSClient> GetKmsClient(
      const std::shared_ptr<Aws::Auth::AWSCredentials>& aws_credentials,
      const std::shared_ptr<std::string>& kms_region) noexcept;

  /// Credentials provider.
  const std::shared_ptr<RoleCredentialsProviderInterface>
      role_credentials_provider_;

  /// The instance of the io async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
};
}  // namespace google::scp::cpio::client_providers
