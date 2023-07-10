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

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/errors.h"
#include "core/interface/http_client_interface.h"
#include "core/utils/src/error_utils.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::ListPrivateKeysByIdsRequest;
using google::scp::cpio::ListPrivateKeysByIdsResponse;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::PrivateKeyClientProviderFactory;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

static constexpr char kPrivateKeyClient[] = "PrivateKeyClient";

namespace google::scp::cpio {
ExecutionResult PrivateKeyClient::Init() noexcept {
  shared_ptr<HttpClientInterface> http_client;
  auto execution_result =
      GlobalCpio::GetGlobalCpio()->GetHttpClient(http_client);
  if (!execution_result.Successful()) {
    ERROR(kPrivateKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get http client.");
    return execution_result;
  }
  shared_ptr<RoleCredentialsProviderInterface> role_credentials_provider;
  execution_result = GlobalCpio::GetGlobalCpio()->GetRoleCredentialsProvider(
      role_credentials_provider);
  if (!execution_result.Successful()) {
    ERROR(kPrivateKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get role credentials provider.");
    return execution_result;
  }
  private_key_client_provider_ = PrivateKeyClientProviderFactory::Create(
      options_, http_client, role_credentials_provider);
  execution_result = private_key_client_provider_->Init();
  if (!execution_result.Successful()) {
    ERROR(kPrivateKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to initialize PrivateKeyClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PrivateKeyClient::Run() noexcept {
  auto execution_result = private_key_client_provider_->Run();
  if (!execution_result.Successful()) {
    ERROR(kPrivateKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to run PrivateKeyClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PrivateKeyClient::Stop() noexcept {
  auto execution_result = private_key_client_provider_->Stop();
  if (!execution_result.Successful()) {
    ERROR(kPrivateKeyClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to stop PrivateKeyClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

void PrivateKeyClient::OnListPrivateKeysByIdsCallback(
    const ListPrivateKeysByIdsRequest& request,
    Callback<ListPrivateKeysByIdsResponse>& callback,
    AsyncContext<
        cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest,
        cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse>&
        list_private_keys_context) noexcept {
  if (!list_private_keys_context.result.Successful()) {
    ERROR_CONTEXT(kPrivateKeyClient, list_private_keys_context,
                  list_private_keys_context.result,
                  "Failed to list private keys by IDs.");
  }
  ListPrivateKeysByIdsResponse response;
  if (list_private_keys_context.response) {
    for (auto private_key :
         list_private_keys_context.response->private_keys()) {
      response.private_keys.emplace_back(PrivateKey(
          {.key_id = private_key.key_id(),
           .public_key = private_key.public_key(),
           .private_key = private_key.private_key(),
           .expiration_time_in_ms = private_key.expiration_time_in_ms()}));
    }
  }
  callback(ConvertToPublicExecutionResult(list_private_keys_context.result),
           move(response));
}

core::ExecutionResult PrivateKeyClient::ListPrivateKeysByIds(
    ListPrivateKeysByIdsRequest request,
    Callback<ListPrivateKeysByIdsResponse> callback) noexcept {
  auto proto_request = make_shared<
      cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest>();
  for (auto key_id : request.key_ids) {
    proto_request->add_key_ids(key_id);
  }

  AsyncContext<cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest,
               cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse>
      list_private_keys_context(
          move(proto_request),
          bind(&PrivateKeyClient::OnListPrivateKeysByIdsCallback, this, request,
               callback, _1),
          kZeroUuid);

  return ConvertToPublicExecutionResult(
      private_key_client_provider_->ListPrivateKeysByIds(
          list_private_keys_context));
}

std::unique_ptr<PrivateKeyClientInterface> PrivateKeyClientFactory::Create(
    PrivateKeyClientOptions options) {
  return make_unique<PrivateKeyClient>(
      make_shared<PrivateKeyClientOptions>(options));
}
}  // namespace google::scp::cpio
