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

#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc PrivateKeyClientInterface
 */
class PrivateKeyClient : public PrivateKeyClientInterface {
 public:
  explicit PrivateKeyClient(
      const std::shared_ptr<PrivateKeyClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult ListPrivateKeys(
      cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest request,
      Callback<cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>
          callback) noexcept override;

 protected:
  virtual core::ExecutionResult CreatePrivateKeyClientProvider() noexcept;

  std::shared_ptr<client_providers::PrivateKeyClientProviderInterface>
      private_key_client_provider_;

 private:
  std::shared_ptr<PrivateKeyClientOptions> options_;
};
}  // namespace google::scp::cpio
