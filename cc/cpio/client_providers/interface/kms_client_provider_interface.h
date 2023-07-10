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
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
/// Represents a KMS request object.
struct KmsDecryptRequest {
  /// AccountIdentity. For AWS, it would be the IAM role.
  std::shared_ptr<AccountIdentity> account_identity;
  /// The region where we want to use the KMS.
  std::shared_ptr<std::string> kms_region;
  /// The key Arn.
  std::shared_ptr<std::string> key_arn;
  /// The ciphertext.
  std::shared_ptr<std::string> ciphertext;
};

/// Represents a KMS response object.
struct KmsDecryptResponse {
  /// The plaintext.
  std::shared_ptr<std::string> plaintext;
};

/**
 * @brief Interface responsible for fetching KMS Aead.
 */
class KmsClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~KmsClientProviderInterface() = default;
  /**
   * @brief Decrypt cipher text using KMS Client.
   *
   * @param decrypt_context decrypt_context of the operation.
   * @return core::ExecutionResult execution result.
   */
  virtual core::ExecutionResult Decrypt(
      core::AsyncContext<KmsDecryptRequest, KmsDecryptResponse>&
          decrypt_context) noexcept = 0;
};

class KmsClientProviderFactory {
 public:
  /**
   * @brief Factory to create KmsClientProvider.
   *
   * @return std::shared_ptr<KmsClientProviderInterface> created
   * KmsClientProvider.
   */
  static std::shared_ptr<KmsClientProviderInterface> Create(
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider);
};
}  // namespace google::scp::cpio::client_providers
