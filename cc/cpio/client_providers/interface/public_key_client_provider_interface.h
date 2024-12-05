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

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/interface/service_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/public_key_client/type_def.h"
#include "cc/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching public keys.
 */
class PublicKeyClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~PublicKeyClientProviderInterface() = default;
  /**
   * @brief Fetches list of public keys.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult ListPublicKeys(
      core::AsyncContext<
          cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
          cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>&
          context) noexcept = 0;
};

class PublicKeyClientProviderFactory {
 public:
  /**
   * @brief Factory to create PublicKeyClientProvider.
   *
   * @return std::shared_ptr<PublicKeyClientProviderInterface> created
   * PublicKeyClientProvider.
   */
  static std::shared_ptr<PublicKeyClientProviderInterface> Create(
      const std::shared_ptr<PublicKeyClientOptions>& options,
      const std::shared_ptr<core::HttpClientInterface>& http_client);
};
}  // namespace google::scp::cpio::client_providers
