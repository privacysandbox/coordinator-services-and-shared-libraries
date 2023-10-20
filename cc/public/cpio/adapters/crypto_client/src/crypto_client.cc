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

#include "crypto_client.h"

#include <memory>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/errors.h"
#include "core/utils/src/error_utils.h"
#include "cpio/client_providers/crypto_client_provider/src/crypto_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/common/adapter_utils.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::CryptoClientProvider;
using google::scp::cpio::client_providers::CryptoClientProviderInterface;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::placeholders::_1;

namespace {
constexpr char kCryptoClient[] = "CryptoClient";
}

namespace google::scp::cpio {
CryptoClient::CryptoClient(const std::shared_ptr<CryptoClientOptions>& options)
    : options_(options) {
  crypto_client_provider_ = make_shared<CryptoClientProvider>(options_);
}

ExecutionResult CryptoClient::Init() noexcept {
  auto execution_result = crypto_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kCryptoClient, kZeroUuid, execution_result,
              "Failed to initialize CryptoClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult CryptoClient::Run() noexcept {
  auto execution_result = crypto_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kCryptoClient, kZeroUuid, execution_result,
              "Failed to run CryptoClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult CryptoClient::Stop() noexcept {
  auto execution_result = crypto_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kCryptoClient, kZeroUuid, execution_result,
              "Failed to stop CryptoClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

core::ExecutionResult CryptoClient::HpkeEncrypt(
    HpkeEncryptRequest request,
    Callback<HpkeEncryptResponse> callback) noexcept {
  return Execute<HpkeEncryptRequest, HpkeEncryptResponse>(
      bind(&CryptoClientProviderInterface::HpkeEncrypt, crypto_client_provider_,
           _1),
      request, callback);
}

core::ExecutionResult CryptoClient::HpkeDecrypt(
    HpkeDecryptRequest request,
    Callback<HpkeDecryptResponse> callback) noexcept {
  return Execute<HpkeDecryptRequest, HpkeDecryptResponse>(
      bind(&CryptoClientProviderInterface::HpkeDecrypt, crypto_client_provider_,
           _1),
      request, callback);
}

core::ExecutionResult CryptoClient::AeadEncrypt(
    AeadEncryptRequest request,
    Callback<AeadEncryptResponse> callback) noexcept {
  return Execute<AeadEncryptRequest, AeadEncryptResponse>(
      bind(&CryptoClientProviderInterface::AeadEncrypt, crypto_client_provider_,
           _1),
      request, callback);
}

core::ExecutionResult CryptoClient::AeadDecrypt(
    AeadDecryptRequest request,
    Callback<AeadDecryptResponse> callback) noexcept {
  return Execute<AeadDecryptRequest, AeadDecryptResponse>(
      bind(&CryptoClientProviderInterface::AeadDecrypt, crypto_client_provider_,
           _1),
      request, callback);
}

std::unique_ptr<CryptoClientInterface> CryptoClientFactory::Create(
    CryptoClientOptions options) {
  return make_unique<CryptoClient>(make_shared<CryptoClientOptions>(options));
}
}  // namespace google::scp::cpio
