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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_types.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
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
      const std::shared_ptr<PrivateKeyFetcherProviderInterface>&
          private_key_fetcher,
      const std::shared_ptr<KmsClientProviderInterface>& kms_client)
      : private_key_client_options_(private_key_client_options),
        private_key_fetcher_(private_key_fetcher),
        kms_client_provider_(kms_client) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult ListPrivateKeys(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          context) noexcept override;

 protected:
  /// Which kind of filter to apply when listing private keys.
  enum class ListingMethod {
    kByKeyId = 1,
    kByMaxAge = 2,
  };

  /// The overrall status of the whole ListPrivateKeys call.
  struct ListPrivateKeysStatus {
    ListPrivateKeysStatus()
        : total_key_split_count(0),
          finished_key_split_count(0),
          fetching_call_returned_count(0),
          got_failure(false),
          got_empty_key_list(false) {}

    virtual ~ListPrivateKeysStatus() = default;

    ListingMethod listing_method = ListingMethod::kByKeyId;

    /// How many keys we are expecting.
    uint64_t expected_total_key_count;

    uint64_t call_count_per_endpoint;

    /// Map of all PrivateKeys to Key IDs.
    std::map<std::string, cmrt::sdk::private_key_service::v1::PrivateKey>
        private_key_id_map;

    /// How many key splits fetched in total.
    std::atomic<size_t> total_key_split_count;
    /// How many key splits finished decryption.
    std::atomic<size_t> finished_key_split_count;
    /// How many fetching call returned.
    std::atomic<size_t> fetching_call_returned_count;

    /// whether ListPrivateKeys() call got failure result.
    std::atomic<bool> got_failure;

    /// whether an empty key list was returned from any endpoint.
    std::atomic<bool> got_empty_key_list;
  };

  /// Operation status for a batch of calls to all endpoints. When listing keys
  /// by IDs, it keeps the status of the calls to all endpoints for one key ID.
  /// When listing keys by age, it keeps the status of the calls to all
  /// endpoints by age.
  struct KeyEndPointsStatus {
    KeyEndPointsStatus() {}

    virtual ~KeyEndPointsStatus() = default;

    /// Map of the plaintexts to key IDs.
    std::map<std::string, std::vector<std::string>> plaintext_key_id_map;

    /// How many endpoints finished operation for current key.
    std::map<std::string, std::atomic<size_t>> finished_counter_key_id_map;

    // Mutex to make sure only one thread accessing the plaintext_key_id_map and
    // finished_counter_key_id_map at one time.
    // Use Mutex other than concurrent map because atomic cannot be stored in
    // the concurrent map.
    std::mutex map_mutex;
  };

  /**
   * @brief Is called after FetchPrivateKey is completed.
   *
   * @param list_private_keys_context ListPrivateKeys context.
   * @param fetch_private_key_context FetchPrivateKey context.
   * @param kms_client KMS client instance for current endpoint.
   * @param list_keys_status ListPrivateKeys operation status.
   * @param endpoints_status operation status of endpoints for one key.
   * @param uri_index endpoint index in endpoints vector.
   *
   */
  virtual void OnFetchPrivateKeyCallback(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          list_private_keys_context,
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          fetch_private_key_context,
      std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
      std::shared_ptr<KeyEndPointsStatus> endpoints_status,
      size_t uri_index) noexcept;

  /**
   * @brief Is called after Decrpyt is completed.
   *
   * @param list_private_keys_context ListPrivateKeys context.
   * @param decrypt_context KMS client Decrypt context.
   * @param list_keys_status ListPrivateKeys operation status.
   * @param endpoints_status operation status of endpoints for one key.
   * @param uri_index endpoint index in endpoints vector.
   *
   */
  virtual void OnDecrpytCallback(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          list_private_keys_context,
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context,
      std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
      std::shared_ptr<KeyEndPointsStatus> endpoints_status,
      std::shared_ptr<EncryptionKey> encryption_key, size_t uri_index) noexcept;

  /// Configurations for PrivateKeyClient.
  std::shared_ptr<PrivateKeyClientOptions> private_key_client_options_;

  /// The private key fetching client instance.
  std::shared_ptr<PrivateKeyFetcherProviderInterface> private_key_fetcher_;

  /// KMS client provider.
  std::shared_ptr<KmsClientProviderInterface> kms_client_provider_;

  // This is temp way to collect all endpoints in one vector. Maybe we should
  // change PrivateKeyClientOptions structure to make thing easy.
  std::vector<PrivateKeyVendingEndpoint> endpoint_list_;
  size_t endpoint_count_;
};
}  // namespace google::scp::cpio::client_providers
