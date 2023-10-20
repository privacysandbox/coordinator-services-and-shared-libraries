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

#include <tink/hybrid/internal/hpke_context.h>

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/crypto_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/**
 * @copydoc CryptoClientProviderInterface
 */
class CryptoClientProvider : public CryptoClientProviderInterface {
 public:
  explicit CryptoClientProvider(
      const std::shared_ptr<CryptoClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult HpkeEncrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::HpkeEncryptRequest,
                         cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>&
          context) noexcept override;

  core::ExecutionResult HpkeDecrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::HpkeDecryptRequest,
                         cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>&
          context) noexcept override;

  core::ExecutionResult AeadEncrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::AeadEncryptRequest,
                         cmrt::sdk::crypto_service::v1::AeadEncryptResponse>&
          context) noexcept override;

  core::ExecutionResult AeadDecrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::AeadDecryptRequest,
                         cmrt::sdk::crypto_service::v1::AeadDecryptResponse>&
          context) noexcept override;

 protected:
  /// HpkeParams passed in from configuration which will override the default
  /// params.
  std::shared_ptr<CryptoClientOptions> options_;
};
}  // namespace google::scp::cpio::client_providers
