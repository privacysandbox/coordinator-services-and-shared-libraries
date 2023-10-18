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

#ifndef SCP_CPIO_INTERFACE_CRYPTO_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_CRYPTO_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for encrypting and decrypting data.
 *
 * Use CryptoClientFactory::Create to create the CryptoClient. Call
 * CryptoClientInterface::Init and CryptoClientInterface::Run before
 * actually use it, and call CryptoClientInterface::Stop when finish using
 * it.
 */
class CryptoClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Encypts payload using HPKE.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult HpkeEncrypt(
      cmrt::sdk::crypto_service::v1::HpkeEncryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>
          callback) noexcept = 0;

  /**
   * @brief Decrypts payload using HPKE.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult HpkeDecrypt(
      cmrt::sdk::crypto_service::v1::HpkeDecryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>
          callback) noexcept = 0;

  /**
   * @brief Encrypts payload using Aead.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult AeadEncrypt(
      cmrt::sdk::crypto_service::v1::AeadEncryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::AeadEncryptResponse>
          callback) noexcept = 0;

  /**
   * @brief Decrypts payload using Aead.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult AeadDecrypt(
      cmrt::sdk::crypto_service::v1::AeadDecryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::AeadDecryptResponse>
          callback) noexcept = 0;
};

/// Factory to create CryptoClient.
class CryptoClientFactory {
 public:
  /**
   * @brief Creates CryptoClient.
   *
   * @param options configurations for CryptoClient.
   * @return std::unique_ptr<CryptoClientInterface> CryptoClient object.
   */
  static std::unique_ptr<CryptoClientInterface> Create(
      CryptoClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_CRYPTO_CLIENT_INTERFACE_H_
