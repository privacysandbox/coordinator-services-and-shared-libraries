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

#include "private_key_client.h"

#include <memory>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/errors.h"
#include "core/interface/http_client_interface.h"
#include "core/utils/src/error_utils.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/common/adapter_utils.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::PrivateKeyClientProviderFactory;
using google::scp::cpio::client_providers::PrivateKeyClientProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

static constexpr char kPrivateKeyClient[] = "PrivateKeyClient";

namespace google::scp::cpio {
ExecutionResult PrivateKeyClient::CreatePrivateKeyClientProvider() noexcept {
  shared_ptr<HttpClientInterface> http_client;
  auto execution_result =
      GlobalCpio::GetGlobalCpio()->GetHttpClient(http_client);
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to get http client.");
    return execution_result;
  }
  shared_ptr<RoleCredentialsProviderInterface> role_credentials_provider;
  execution_result = GlobalCpio::GetGlobalCpio()->GetRoleCredentialsProvider(
      role_credentials_provider);
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to get role credentials provider.");
    return execution_result;
  }
  shared_ptr<AuthTokenProviderInterface> auth_token_provider;
  execution_result =
      GlobalCpio::GetGlobalCpio()->GetAuthTokenProvider(auth_token_provider);
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to get role auth token provider.");
    return execution_result;
  }
  shared_ptr<AsyncExecutorInterface> io_async_executor;
  execution_result =
      GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to get IOAsyncExecutor.");
    return execution_result;
  }
  private_key_client_provider_ = PrivateKeyClientProviderFactory::Create(
      options_, http_client, role_credentials_provider, auth_token_provider,
      io_async_executor);
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClient::Init() noexcept {
  auto execution_result = CreatePrivateKeyClientProvider();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to create PrivateKeyClientProvider.");
    return ConvertToPublicExecutionResult(execution_result);
  }

  execution_result = private_key_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to initialize PrivateKeyClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PrivateKeyClient::Run() noexcept {
  auto execution_result = private_key_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to run PrivateKeyClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PrivateKeyClient::Stop() noexcept {
  auto execution_result = private_key_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPrivateKeyClient, kZeroUuid, execution_result,
              "Failed to stop PrivateKeyClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

core::ExecutionResult PrivateKeyClient::ListPrivateKeys(
    ListPrivateKeysRequest request,
    Callback<ListPrivateKeysResponse> callback) noexcept {
  return Execute<ListPrivateKeysRequest, ListPrivateKeysResponse>(
      bind(&PrivateKeyClientProviderInterface::ListPrivateKeys,
           private_key_client_provider_, _1),
      request, callback);
}

std::unique_ptr<PrivateKeyClientInterface> PrivateKeyClientFactory::Create(
    PrivateKeyClientOptions options) {
  return make_unique<PrivateKeyClient>(
      make_shared<PrivateKeyClientOptions>(options));
}
}  // namespace google::scp::cpio
