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

#include "cpio/client_providers/interface/crypto_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc CryptoClientInterface
 */
class CryptoClient : public CryptoClientInterface {
 public:
  explicit CryptoClient(const std::shared_ptr<CryptoClientOptions>& options);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult HpkeEncrypt(
      cmrt::sdk::crypto_service::v1::HpkeEncryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>
          callback) noexcept override;

  core::ExecutionResult HpkeDecrypt(
      cmrt::sdk::crypto_service::v1::HpkeDecryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>
          callback) noexcept override;

  core::ExecutionResult AeadEncrypt(
      cmrt::sdk::crypto_service::v1::AeadEncryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::AeadEncryptResponse>
          callback) noexcept override;

  core::ExecutionResult AeadDecrypt(
      cmrt::sdk::crypto_service::v1::AeadDecryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::AeadDecryptResponse>
          callback) noexcept override;

 protected:
  std::shared_ptr<client_providers::CryptoClientProviderInterface>
      crypto_client_provider_;
  std::shared_ptr<CryptoClientOptions> options_;
};
}  // namespace google::scp::cpio
