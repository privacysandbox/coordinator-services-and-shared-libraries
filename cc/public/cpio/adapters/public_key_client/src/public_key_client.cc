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
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::ListPublicKeysRequest;
using google::scp::cpio::ListPublicKeysResponse;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::PublicKeyClientProviderFactory;
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
    ERROR(kPublicKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get http client.");
  }
  public_key_client_provider_ =
      PublicKeyClientProviderFactory::Create(options_, http_client);
  return SuccessExecutionResult();
}

ExecutionResult PublicKeyClient::Init() noexcept {
  auto execution_result = CreatePublicKeyClientProvider();
  if (!execution_result.Successful()) {
    ERROR(kPublicKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to create PublicKeyClientProvider.");
    return ConvertToPublicExecutionResult(execution_result);
  }

  execution_result = public_key_client_provider_->Init();
  if (!execution_result.Successful()) {
    ERROR(kPublicKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to initialize PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PublicKeyClient::Run() noexcept {
  auto execution_result = public_key_client_provider_->Run();
  if (!execution_result.Successful()) {
    ERROR(kPublicKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to run PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PublicKeyClient::Stop() noexcept {
  auto execution_result = public_key_client_provider_->Stop();
  if (!execution_result.Successful()) {
    ERROR(kPublicKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to stop PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

void PublicKeyClient::OnListPublicKeysCallback(
    const ListPublicKeysRequest& request,
    Callback<ListPublicKeysResponse>& callback,
    AsyncContext<cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
                 cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>&
        list_public_keys_context) noexcept {
  if (!list_public_keys_context.result.Successful()) {
    ERROR_CONTEXT(kPublicKeyClient, list_public_keys_context,
                  list_public_keys_context.result,
                  "Failed to list public keys.");
  }
  ListPublicKeysResponse response;
  if (list_public_keys_context.response) {
    for (auto public_key : list_public_keys_context.response->public_keys()) {
      response.public_keys.emplace_back(
          PublicKey({.key_id = public_key.key_id(),
                     .public_key = public_key.public_key()}));
    }
    response.expiration_time_in_ms =
        list_public_keys_context.response->expiration_time_in_ms();
  }
  callback(ConvertToPublicExecutionResult(list_public_keys_context.result),
           move(response));
}

core::ExecutionResult PublicKeyClient::ListPublicKeys(
    ListPublicKeysRequest request,
    Callback<ListPublicKeysResponse> callback) noexcept {
  auto proto_request =
      make_shared<cmrt::sdk::public_key_service::v1::ListPublicKeysRequest>();

  AsyncContext<cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
               cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>
      list_public_keys_context(move(proto_request),
                               bind(&PublicKeyClient::OnListPublicKeysCallback,
                                    this, request, callback, _1),
                               kZeroUuid);

  return ConvertToPublicExecutionResult(
      public_key_client_provider_->ListPublicKeys(list_public_keys_context));
}

std::unique_ptr<PublicKeyClientInterface> PublicKeyClientFactory::Create(
    PublicKeyClientOptions options) {
  return make_unique<PublicKeyClient>(
      make_shared<PublicKeyClientOptions>(options));
}
}  // namespace google::scp::cpio
