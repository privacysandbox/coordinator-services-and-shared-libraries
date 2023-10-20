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

#include "kms_client.h"

#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::KmsClientProviderFactory;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;

namespace {
constexpr char kKmsClient[] = "KmsClient";
}  // namespace

namespace google::scp::cpio {

ExecutionResult KmsClient::Init() noexcept {
  shared_ptr<RoleCredentialsProviderInterface> role_credentials_provider;
  auto execution_result =
      GlobalCpio::GetGlobalCpio()->GetRoleCredentialsProvider(
          role_credentials_provider);
  if (!execution_result.Successful()) {
    SCP_ERROR(kKmsClient, kZeroUuid, execution_result,
              "Failed to get RoleCredentialsProvider.");
    return execution_result;
  }
  shared_ptr<AsyncExecutorInterface> io_async_executor;
  execution_result =
      GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kKmsClient, kZeroUuid, execution_result,
              "Failed to get IOAsyncExecutor.");
    return execution_result;
  }
  kms_client_provider_ = KmsClientProviderFactory::Create(
      options_, role_credentials_provider, io_async_executor);
  execution_result = kms_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kKmsClient, kZeroUuid, execution_result,
              "Failed to initialize KmsClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult KmsClient::Run() noexcept {
  auto execution_result = kms_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kKmsClient, kZeroUuid, execution_result,
              "Failed to run KmsClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult KmsClient::Stop() noexcept {
  auto execution_result = kms_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kKmsClient, kZeroUuid, execution_result,
              "Failed to stop KmsClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult KmsClient::Decrypt(
    AsyncContext<DecryptRequest, DecryptResponse> decrypt_context) noexcept {
  return kms_client_provider_->Decrypt(decrypt_context);
}

std::unique_ptr<KmsClientInterface> KmsClientFactory::Create(
    KmsClientOptions options) {
  return make_unique<KmsClient>(make_shared<KmsClientOptions>(move(options)));
}
}  // namespace google::scp::cpio
