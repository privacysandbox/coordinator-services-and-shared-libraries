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

#include "config_client_provider.h"

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/config_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/type_def.h"
#include "cpio/proto/config_client.pb.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/config_client/type_def.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_CONFIG_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
using google::scp::core::errors::SC_CONFIG_CLIENT_PROVIDER_TAG_NOT_FOUND;
using google::scp::cpio::config_client::GetInstanceIdProtoRequest;
using google::scp::cpio::config_client::GetInstanceIdProtoResponse;
using google::scp::cpio::config_client::GetTagProtoRequest;
using google::scp::cpio::config_client::GetTagProtoResponse;
using std::bind;
using std::make_shared;
using std::map;
using std::move;
using std::pair;
using std::promise;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;

/// Filename for logging errors
static constexpr char kConfigClientProvider[] = "ConfigClientProvider";

namespace google::scp::cpio::client_providers {
ConfigClientProvider::ConfigClientProvider(
    const shared_ptr<ConfigClientOptions>& config_client_options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider)
    : instance_client_provider_(instance_client_provider) {
  parameter_client_provider_ = ParameterClientProviderFactory::Create(
      make_shared<ParameterClientOptions>(), instance_client_provider_);
}

ExecutionResult ConfigClientProvider::Init() noexcept {
  auto execution_result = parameter_client_provider_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult ConfigClientProvider::Run() noexcept {
  auto execution_result = parameter_client_provider_->Run();
  if (!execution_result.Successful()) {
    ERROR(kConfigClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to run InstanceClientProvider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult ConfigClientProvider::Stop() noexcept {
  auto execution_result = parameter_client_provider_->Stop();
  if (!execution_result.Successful()) {
    ERROR(kConfigClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to stop InstanceClientProvider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult ConfigClientProvider::GetParameter(
    AsyncContext<GetParameterRequest, GetParameterResponse>& context) noexcept {
  AsyncContext<GetParameterRequest, GetParameterResponse>
      parameter_client_context(
          move(context.request),
          bind(&ConfigClientProvider::OnGetParameterCallback, this, context,
               _1));

  return parameter_client_provider_->GetParameter(parameter_client_context);
}

void ConfigClientProvider::OnGetParameterCallback(
    core::AsyncContext<GetParameterRequest, GetParameterResponse>&
        config_client_context,
    core::AsyncContext<GetParameterRequest, GetParameterResponse>&
        parameter_client_context) noexcept {
  config_client_context.result = parameter_client_context.result;
  if (!config_client_context.result.Successful()) {
    ERROR_CONTEXT(kConfigClientProvider, config_client_context,
                  config_client_context.result,
                  "Failed to get parameter for name %s.",
                  parameter_client_context.request->parameter_name().c_str());
    config_client_context.Finish();
    return;
  }
  config_client_context.response = move(parameter_client_context.response);
  config_client_context.Finish();
}

ExecutionResult ConfigClientProvider::GetInstanceId(
    AsyncContext<GetInstanceIdProtoRequest, GetInstanceIdProtoResponse>&
        context) noexcept {
  string instance_id;
  auto execution_result =
      instance_client_provider_->GetCurrentInstanceId(instance_id);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT(kConfigClientProvider, context, execution_result,
                  "Failed getting AWS instance ID.");
    context.result = execution_result;
    context.Finish();
    return execution_result;
  }

  context.response = make_shared<GetInstanceIdProtoResponse>();
  context.response->set_instance_id(instance_id);
  context.result = SuccessExecutionResult();
  context.Finish();

  return SuccessExecutionResult();
}

ExecutionResult ConfigClientProvider::GetTag(
    AsyncContext<GetTagProtoRequest, GetTagProtoResponse>& context) noexcept {
  string instance_id;
  auto execution_result =
      instance_client_provider_->GetCurrentInstanceId(instance_id);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT(kConfigClientProvider, context, execution_result,
                  "Failed getting AWS instance ID.");
    context.result = execution_result;
    context.Finish();
    return execution_result;
  }

  vector<string> tag_names(1);
  tag_names.emplace_back(context.request->tag_name());
  map<string, string> tag_values_map;
  execution_result = instance_client_provider_->GetTagsOfInstance(
      tag_names, instance_id, tag_values_map);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT(kConfigClientProvider, context, execution_result,
                  "Failed getting instance tag for name %s.",
                  context.request->tag_name().c_str());
    context.result = execution_result;
    context.Finish();
    return execution_result;
  }

  context.result = SuccessExecutionResult();
  context.response = make_shared<GetTagProtoResponse>();
  context.response->set_value(tag_values_map.at(context.request->tag_name()));
  context.Finish();

  return context.result;
}

#ifndef TEST_CPIO
shared_ptr<ConfigClientProviderInterface> ConfigClientProviderFactory::Create(
    const shared_ptr<ConfigClientOptions>& options) {
  std::shared_ptr<InstanceClientProviderInterface> instance_client;
  GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(instance_client);
  return make_shared<ConfigClientProvider>(options, instance_client);
}
#endif
}  // namespace google::scp::cpio::client_providers
