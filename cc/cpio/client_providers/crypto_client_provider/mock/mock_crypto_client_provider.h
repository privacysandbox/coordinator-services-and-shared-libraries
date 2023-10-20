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

#include <gmock/gmock.h>

#include <memory>

#include "cpio/client_providers/interface/crypto_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockCryptoClientProvider : public CryptoClientProviderInterface {
 public:
  MOCK_METHOD(core::ExecutionResult, Init, (), (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, Run, (), (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (override, noexcept));

  MOCK_METHOD(core::ExecutionResult, HpkeEncrypt,
              ((core::AsyncContext<
                  cmrt::sdk::crypto_service::v1::HpkeEncryptRequest,
                  cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>&)),
              (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, HpkeDecrypt,
              ((core::AsyncContext<
                  cmrt::sdk::crypto_service::v1::HpkeDecryptRequest,
                  cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>&)),
              (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, AeadEncrypt,
              ((core::AsyncContext<
                  cmrt::sdk::crypto_service::v1::AeadEncryptRequest,
                  cmrt::sdk::crypto_service::v1::AeadEncryptResponse>&)),
              (override, noexcept));
  MOCK_METHOD(core::ExecutionResult, AeadDecrypt,
              ((core::AsyncContext<
                  cmrt::sdk::crypto_service::v1::AeadDecryptRequest,
                  cmrt::sdk::crypto_service::v1::AeadDecryptResponse>&)),
              (override, noexcept));
};
}  // namespace google::scp::cpio::client_providers::mock
