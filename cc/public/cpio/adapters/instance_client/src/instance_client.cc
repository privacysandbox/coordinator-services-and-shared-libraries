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

#include "instance_client.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/common/adapter_utils.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderFactory;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::string;
using std::placeholders::_1;

static constexpr char kInstanceClient[] = "InstanceClient";

namespace google::scp::cpio {
ExecutionResult InstanceClient::CreateInstanceClientProvider() noexcept {
  auto execution_result =
      GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(
          instance_client_provider_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceClient, kZeroUuid, execution_result,
              "Failed to get InstanceClientProvider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult InstanceClient::Init() noexcept {
  auto execution_result = CreateInstanceClientProvider();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceClient, kZeroUuid, execution_result,
              "Failed to create InstanceClientProvider.");
    return ConvertToPublicExecutionResult(execution_result);
  }

  return SuccessExecutionResult();
}

ExecutionResult InstanceClient::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult InstanceClient::Stop() noexcept {
  return SuccessExecutionResult();
}

core::ExecutionResult InstanceClient::GetCurrentInstanceResourceName(
    GetCurrentInstanceResourceNameRequest request,
    Callback<GetCurrentInstanceResourceNameResponse> callback) noexcept {
  return Execute<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>(
      bind(&InstanceClientProviderInterface::GetCurrentInstanceResourceName,
           instance_client_provider_, _1),
      request, callback);
}

core::ExecutionResult InstanceClient::GetTagsByResourceName(
    GetTagsByResourceNameRequest request,
    Callback<GetTagsByResourceNameResponse> callback) noexcept {
  return Execute<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>(
      bind(&InstanceClientProviderInterface::GetTagsByResourceName,
           instance_client_provider_, _1),
      request, callback);
}

core::ExecutionResult InstanceClient::GetInstanceDetailsByResourceName(
    GetInstanceDetailsByResourceNameRequest request,
    Callback<GetInstanceDetailsByResourceNameResponse> callback) noexcept {
  return Execute<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>(
      bind(&InstanceClientProviderInterface::GetInstanceDetailsByResourceName,
           instance_client_provider_, _1),
      request, callback);
}

std::unique_ptr<InstanceClientInterface> InstanceClientFactory::Create(
    InstanceClientOptions options) {
  return make_unique<InstanceClient>(
      make_shared<InstanceClientOptions>(options));
}
}  // namespace google::scp::cpio
