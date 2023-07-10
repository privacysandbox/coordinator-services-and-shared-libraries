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

#include "config_client.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/proto/config_client.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::errors::SC_CONFIG_CLIENT_INVALID_PARAMETER_NAME;
using google::scp::cpio::GetInstanceIdRequest;
using google::scp::cpio::GetInstanceIdResponse;
using google::scp::cpio::GetParameterRequest;
using google::scp::cpio::GetParameterResponse;
using google::scp::cpio::GetTagRequest;
using google::scp::cpio::GetTagResponse;
using google::scp::cpio::client_providers::ConfigClientProviderFactory;
using google::scp::cpio::config_client::GetInstanceIdProtoRequest;
using google::scp::cpio::config_client::GetInstanceIdProtoResponse;
using google::scp::cpio::config_client::GetTagProtoRequest;
using google::scp::cpio::config_client::GetTagProtoResponse;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::string;
using std::placeholders::_1;

static constexpr char kConfigClient[] = "ConfigClient";

namespace google::scp::cpio {
ConfigClient::ConfigClient(
    const std::shared_ptr<ConfigClientOptions>& options) {
  config_client_provider_ = ConfigClientProviderFactory::Create(options);
}

ExecutionResult ConfigClient::Init() noexcept {
  auto execution_result = config_client_provider_->Init();
  if (!execution_result.Successful()) {
    ERROR(kConfigClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to initialize ConfigClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult ConfigClient::Run() noexcept {
  auto execution_result = config_client_provider_->Run();
  if (!execution_result.Successful()) {
    ERROR(kConfigClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to run ConfigClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult ConfigClient::Stop() noexcept {
  auto execution_result = config_client_provider_->Stop();
  if (!execution_result.Successful()) {
    ERROR(kConfigClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to stop ConfigClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

void ConfigClient::OnGetParameterCallback(
    const GetParameterRequest& request,
    Callback<GetParameterResponse>& callback,
    AsyncContext<cmrt::sdk::parameter_service::v1::GetParameterRequest,
                 cmrt::sdk::parameter_service::v1::GetParameterResponse>&
        get_parameter_context) noexcept {
  if (!get_parameter_context.result.Successful()) {
    ERROR_CONTEXT(
        kConfigClient, get_parameter_context, get_parameter_context.result,
        "Failed to get parameter for %s.", request.parameter_name.c_str());
    uint64_t public_error_code =
        GetPublicErrorCode(get_parameter_context.result.status_code);
    get_parameter_context.result.status_code = public_error_code;
    callback(get_parameter_context.result, GetParameterResponse());
    return;
  }
  GetParameterResponse response;
  response.parameter_value = get_parameter_context.response->parameter_value();
  callback(get_parameter_context.result, std::move(response));
}

core::ExecutionResult ConfigClient::GetParameter(
    GetParameterRequest request,
    Callback<GetParameterResponse> callback) noexcept {
  if (request.parameter_name.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_CONFIG_CLIENT_INVALID_PARAMETER_NAME);
    ERROR(kConfigClient, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get parameter for %s.", request.parameter_name.c_str());
    callback(execution_result, GetParameterResponse());
    return execution_result;
  }

  auto proto_request =
      make_shared<cmrt::sdk::parameter_service::v1::GetParameterRequest>();
  proto_request->set_parameter_name(request.parameter_name);

  AsyncContext<cmrt::sdk::parameter_service::v1::GetParameterRequest,
               cmrt::sdk::parameter_service::v1::GetParameterResponse>
      get_parameter_context(move(proto_request),
                            bind(&ConfigClient::OnGetParameterCallback, this,
                                 request, callback, _1),
                            kZeroUuid);

  return config_client_provider_->GetParameter(get_parameter_context);
}

void ConfigClient::OnGetTagCallback(
    const GetTagRequest& request, Callback<GetTagResponse>& callback,
    AsyncContext<GetTagProtoRequest, GetTagProtoResponse>&
        get_tag_context) noexcept {
  if (!get_tag_context.result.Successful()) {
    ERROR_CONTEXT(kConfigClient, get_tag_context, get_tag_context.result,
                  "Failed to get tag for %s.", request.tag_name.c_str());
    uint64_t public_error_code =
        GetPublicErrorCode(get_tag_context.result.status_code);
    get_tag_context.result.status_code = public_error_code;
    callback(get_tag_context.result, GetTagResponse());
    return;
  }
  GetTagResponse response;
  response.tag_value = get_tag_context.response->value();
  callback(get_tag_context.result, std::move(response));
}

core::ExecutionResult ConfigClient::GetTag(
    GetTagRequest request, Callback<GetTagResponse> callback) noexcept {
  auto proto_request = make_shared<GetTagProtoRequest>();
  proto_request->set_tag_name(request.tag_name);
  AsyncContext<GetTagProtoRequest, GetTagProtoResponse> get_tag_context(
      move(proto_request),
      bind(&ConfigClient::OnGetTagCallback, this, request, callback, _1),
      kZeroUuid);

  return config_client_provider_->GetTag(get_tag_context);
}

void ConfigClient::OnGetInstanceIdCallback(
    const GetInstanceIdRequest& request,
    Callback<GetInstanceIdResponse>& callback,
    AsyncContext<GetInstanceIdProtoRequest, GetInstanceIdProtoResponse>&
        get_instance_id_context) noexcept {
  if (!get_instance_id_context.result.Successful()) {
    ERROR_CONTEXT(kConfigClient, get_instance_id_context,
                  get_instance_id_context.result, "Failed to get instance ID.");
    uint64_t public_error_code =
        GetPublicErrorCode(get_instance_id_context.result.status_code);
    get_instance_id_context.result.status_code = public_error_code;
    callback(get_instance_id_context.result, GetInstanceIdResponse());
    return;
  }
  GetInstanceIdResponse response;
  response.instance_id = get_instance_id_context.response->instance_id();
  callback(get_instance_id_context.result, std::move(response));
}

core::ExecutionResult ConfigClient::GetInstanceId(
    GetInstanceIdRequest request,
    Callback<GetInstanceIdResponse> callback) noexcept {
  auto proto_request = make_shared<GetInstanceIdProtoRequest>();
  AsyncContext<GetInstanceIdProtoRequest, GetInstanceIdProtoResponse>
      get_instance_id_context(move(proto_request),
                              bind(&ConfigClient::OnGetInstanceIdCallback, this,
                                   request, callback, _1),
                              kZeroUuid);

  return config_client_provider_->GetInstanceId(get_instance_id_context);
}

std::unique_ptr<ConfigClientInterface> ConfigClientFactory::Create(
    ConfigClientOptions options) {
  return make_unique<ConfigClient>(make_shared<ConfigClientOptions>(options));
}
}  // namespace google::scp::cpio
