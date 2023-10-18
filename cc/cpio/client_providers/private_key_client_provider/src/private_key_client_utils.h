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
#include <utility>
#include <vector>

#include "core/interface/http_types.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class PrivateKeyClientUtils {
 public:
  /**
   * @brief Get the Kms Decrypt Request object.
   *
   * @param encryption_key EncryptionKey object.
   * @param[out] kms_decrypt_request KmsDecryptRequest object.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetKmsDecryptRequest(
      const std::shared_ptr<EncryptionKey>& encryption_key,
      cmrt::sdk::kms_service::v1::DecryptRequest& kms_decrypt_request) noexcept;

  /**
   * @brief Get the Private Key object with unencrypt information.
   *
   * @param encryption_key EncryptionKey object.
   * @param[out] private_key PrivateKey object for private key split.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetPrivateKeyInfo(
      const std::shared_ptr<EncryptionKey>& encryption_key,
      cmrt::sdk::private_key_service::v1::PrivateKey& private_key) noexcept;

  /**
   * @brief Reconstruct Xor keyset.
   *
   * @param endpoint_responses split private key responses from all endpoints.
   * @param[out] private_key final private key.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ReconstructXorKeysetHandle(
      const std::vector<std::string>& endpoint_responses,
      std::string& private_key) noexcept;
};
}  // namespace google::scp::cpio::client_providers
