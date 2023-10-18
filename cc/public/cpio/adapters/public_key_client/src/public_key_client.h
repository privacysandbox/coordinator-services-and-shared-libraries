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

#include "cpio/client_providers/interface/public_key_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc PublicKeyClientInterface
 */
class PublicKeyClient : public PublicKeyClientInterface {
 public:
  explicit PublicKeyClient(
      const std::shared_ptr<PublicKeyClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult ListPublicKeys(
      cmrt::sdk::public_key_service::v1::ListPublicKeysRequest request,
      Callback<cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>
          callback) noexcept override;

 protected:
  virtual core::ExecutionResult CreatePublicKeyClientProvider() noexcept;

  std::shared_ptr<client_providers::PublicKeyClientProviderInterface>
      public_key_client_provider_;
  std::shared_ptr<PublicKeyClientOptions> options_;
};
}  // namespace google::scp::cpio
