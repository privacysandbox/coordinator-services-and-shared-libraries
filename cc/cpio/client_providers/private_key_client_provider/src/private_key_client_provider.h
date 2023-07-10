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
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_types.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_fetching_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "private_key_client_utils.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc PrivateKeyClientProviderInterface
 */
class PrivateKeyClientProvider : public PrivateKeyClientProviderInterface {
 public:
  virtual ~PrivateKeyClientProvider() = default;

  explicit PrivateKeyClientProvider(
      const std::shared_ptr<PrivateKeyClientOptions>&
          private_key_client_options,
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult ListPrivateKeysByIds(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse>&
          context) noexcept override;

 protected:
  /// Operation status of list of private keys acquire.
  struct ListPrivateKeysStatus {
    ListPrivateKeysStatus() : finished_counter(0), got_failure(false) {}

    virtual ~ListPrivateKeysStatus() = default;

    /// Vector of PrivateKeyResponse for all keys.
    std::vector<cmrt::sdk::private_key_service::v1::PrivateKey> responses;

    /// How many keys finished operation.
    std::atomic<uint64_t> finished_counter;

    /// whether ListPrivateKeysByIds() call got failure result.
    std::atomic<bool> got_failure;
  };

  /// Operation status of one private key acquire.
  struct KeyEndPointsStatus {
    KeyEndPointsStatus() : finished_counter(0) {}

    virtual ~KeyEndPointsStatus() = default;

    /// Vector of the responses from all endpoints for current key.
    std::vector<std::string> responses;

    /// The index of current key id in private keys list.
    size_t key_id_index;

    /// How many endpoints finished operation for current key.
    std::atomic<size_t> finished_counter;
  };

  /**
   * @brief Is called after FetchPrivateKey is completed.
   *
   * @param list_private_keys_context ListPrivateKeysByIds context.
   * @param fetch_private_key_context FetchPrivateKey context.
   * @param kms_client KMS client instance for current endpoint.
   * @param list_keys_status ListPrivateKeysByIds operation status.
   * @param endpoints_status operation status of endpoints for one key.
   * @param uri_index endpoint index in endpoints vector.
   *
   */
  virtual void OnFetchPrivateKeyCallback(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse>&
          list_private_keys_context,
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          fetch_private_key_context,
      std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
      std::shared_ptr<KeyEndPointsStatus> endpoints_status,
      size_t uri_index) noexcept;

  /**
   * @brief Is called after Decrpyt is completed.
   *
   * @param list_private_keys_context ListPrivateKeysByIds context.
   * @param decrypt_context KMS client Decrypt context.
   * @param list_keys_status ListPrivateKeysByIds operation status.
   * @param endpoints_status operation status of endpoints for one key.
   * @param uri_index endpoint index in endpoints vector.
   *
   */
  virtual void OnDecrpytCallback(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse>&
          list_private_keys_context,
      core::AsyncContext<KmsDecryptRequest, KmsDecryptResponse>&
          decrypt_context,
      std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
      std::shared_ptr<KeyEndPointsStatus> endpoints_status,
      size_t uri_index) noexcept;

  /// Configurations for PrivateKeyClient.
  std::shared_ptr<PrivateKeyClientOptions> private_key_client_options_;

  /// The private key fetching client instance.
  std::shared_ptr<PrivateKeyFetchingClientProviderInterface>
      private_key_fetching_client_;

  /// KMS client provider.
  std::shared_ptr<KmsClientProviderInterface> kms_client_provider_;

  // This is temp way to collect all endpoints in one vector. Maybe we should
  // change PrivateKeyClientOptions structure to make thing easy.
  std::vector<PrivateKeyVendingEndpoint> endpoint_list_;
  size_t endpoint_num_;
};
}  // namespace google::scp::cpio::client_providers
