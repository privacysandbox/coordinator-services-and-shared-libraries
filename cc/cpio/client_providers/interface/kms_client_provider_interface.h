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
#include "public/cpio/interface/kms_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

namespace google::scp::cpio::client_providers {

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
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
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
      const std::shared_ptr<KmsClientOptions>& options,
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers
