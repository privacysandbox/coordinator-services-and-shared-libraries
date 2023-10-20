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
#include "core/interface/service_interface.h"
#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/interface/type_def.h"

#include "auth_token_provider_interface.h"
#include "role_credentials_provider_interface.h"

namespace google::scp::cpio::client_providers {

/// Request for fetching private key.
struct PrivateKeyFetchingRequest {
  /// Endpoint to surface the private keys.
  std::shared_ptr<PrivateKeyVendingEndpoint> key_vending_endpoint;

  /// The list of identifiers of the public and private key pair.
  /// If set, do not honor the max_age_seconds.
  std::shared_ptr<std::string> key_id;

  /// Return all keys generated newer than max_age_seconds.
  int max_age_seconds;
};

/// Type of encryption key and how it is split.
enum class EncryptionKeyType {
  /// Single-coordinator managed key using a Tink hybrid key.
  kSinglePartyHybridKey = 0,
  /// Multi-coordinator using a Tink hybrid key, split using XOR with random
  /// data.
  kMultiPartyHybridEvenKeysplit = 1,
};

/// Represents key material and metadata associated with the key.
struct KeyData {
  /**
   * @brief Cryptographic signature of the public key material from the
   * coordinator identified by keyEncryptionKeyUri
   */
  std::shared_ptr<std::string> public_key_signature;

  /**
   * @brief URI of the cloud KMS key used to encrypt the keyMaterial (also used
   * to identify who owns the key material, and the signer of
   * publicKeySignature)
   *
   * e.g. aws-kms://arn:aws:kms:us-east-1:012345678901:key/abcd
   */
  std::shared_ptr<std::string> key_encryption_key_uri;

  /**
   * @brief The encrypted key material, of type defined by EncryptionKeyType of
   * the EncryptionKey
   */
  std::shared_ptr<std::string> key_material;
};

struct EncryptionKey {
  /// Key ID.
  std::shared_ptr<std::string> key_id;
  /**
   * @brief Resource name (see <a href="https://google.aip.dev/122">AIP-122</a>)
   * representing the encryptedPrivateKey. E.g. "privateKeys/{keyid}"
   *
   */
  std::shared_ptr<std::string> resource_name;

  /// The type of key, and how it is split.
  EncryptionKeyType encryption_key_type;

  /// Tink keyset handle containing the public key material.
  std::shared_ptr<std::string> public_keyset_handle;

  /// The raw public key material, base 64 encoded.
  std::shared_ptr<std::string> public_key_material;

  /// Key expiration time in Unix Epoch milliseconds.
  core::Timestamp expiration_time_in_ms;

  /// Key creation time in Unix Epoch milliseconds.
  core::Timestamp creation_time_in_ms;

  /// List of key data. The size of key_data is matched with split parts of
  /// the private key.
  std::vector<std::shared_ptr<KeyData>> key_data;
};

/// Response for fetching private key.
struct PrivateKeyFetchingResponse {
  std::vector<std::shared_ptr<EncryptionKey>> encryption_keys;
};

/**
 * @brief Interface responsible for fetching private key.
 */
class PrivateKeyFetcherProviderInterface : public core::ServiceInterface {
 public:
  virtual ~PrivateKeyFetcherProviderInterface() = default;
  /**
   * @brief Fetches private key.
   *
   * @param context context of the operation.
   * @return core::ExecutionResult execution result.
   */
  virtual core::ExecutionResult FetchPrivateKey(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          context) noexcept = 0;
};

class PrivateKeyFetcherProviderFactory {
 public:
  /**
   * @brief Factory to create PrivateKeyFetcherProvider.
   *
   * @param http_client the HttpClient.
   * @param role_credentials_provider the RoleCredentialsProvider.
   * @return std::shared_ptr<PrivateKeyFetcherProviderInterface> created
   * PrivateKeyFetcherProvider.
   */
  static std::shared_ptr<PrivateKeyFetcherProviderInterface> Create(
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider,
      const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider);
};
}  // namespace google::scp::cpio::client_providers
