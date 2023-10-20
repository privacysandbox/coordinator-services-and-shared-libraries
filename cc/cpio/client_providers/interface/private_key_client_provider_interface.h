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
#include "core/interface/http_client_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching private keys.
 */
class PrivateKeyClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~PrivateKeyClientProviderInterface() = default;
  /**
   * @brief Fetches list of private keys by ids.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult ListPrivateKeys(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          context) noexcept = 0;
};

class PrivateKeyClientProviderFactory {
 public:
  /**
   * @brief Factory to create PrivateKeyClientProvider.
   *
   * @return std::shared_ptr<PrivateKeyClientProviderInterface> created
   * PrivateKeyClientProvider.
   */
  static std::shared_ptr<PrivateKeyClientProviderInterface> Create(
      const std::shared_ptr<PrivateKeyClientOptions>& options,
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider,
      const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);
};
}  // namespace google::scp::cpio::client_providers
