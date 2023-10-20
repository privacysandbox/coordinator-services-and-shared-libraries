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

#include "public_key_client.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/errors.h"
#include "core/utils/src/error_utils.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/common/adapter_utils.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::PublicKeyClientProviderFactory;
using google::scp::cpio::client_providers::PublicKeyClientProviderInterface;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

static constexpr char kPublicKeyClient[] = "PublicKeyClient";

namespace google::scp::cpio {
ExecutionResult PublicKeyClient::CreatePublicKeyClientProvider() noexcept {
  shared_ptr<HttpClientInterface> http_client;
  auto execution_result =
      GlobalCpio::GetGlobalCpio()->GetHttpClient(http_client);
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to get http client.");
  }
  public_key_client_provider_ =
      PublicKeyClientProviderFactory::Create(options_, http_client);
  return SuccessExecutionResult();
}

ExecutionResult PublicKeyClient::Init() noexcept {
  auto execution_result = CreatePublicKeyClientProvider();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to create PublicKeyClientProvider.");
    return ConvertToPublicExecutionResult(execution_result);
  }

  execution_result = public_key_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to initialize PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PublicKeyClient::Run() noexcept {
  auto execution_result = public_key_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to run PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PublicKeyClient::Stop() noexcept {
  auto execution_result = public_key_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to stop PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

core::ExecutionResult PublicKeyClient::ListPublicKeys(
    ListPublicKeysRequest request,
    Callback<ListPublicKeysResponse> callback) noexcept {
  return Execute<ListPublicKeysRequest, ListPublicKeysResponse>(
      bind(&PublicKeyClientProviderInterface::ListPublicKeys,
           public_key_client_provider_, _1),
      request, callback);
}

std::unique_ptr<PublicKeyClientInterface> PublicKeyClientFactory::Create(
    PublicKeyClientOptions options) {
  return make_unique<PublicKeyClient>(
      make_shared<PublicKeyClientOptions>(options));
}
}  // namespace google::scp::cpio
