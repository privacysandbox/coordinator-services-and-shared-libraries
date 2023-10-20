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

#include "aws_parameter_client_provider.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/core/utils/Outcome.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/model/GetParametersRequest.h>

#include "cc/core/common/uuid/src/uuid.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_utils.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"
#include "ssm_error_converter.h"

using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::SSM::SSMClient;
using Aws::SSM::Model::GetParametersOutcome;
using Aws::SSM::Model::GetParametersRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using google::scp::cpio::common::CreateClientConfiguration;
using std::make_shared;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

/// Filename for logging errors
static constexpr char kAwsParameterClientProvider[] =
    "AwsParameterClientProvider";

namespace google::scp::cpio::client_providers {
shared_ptr<ClientConfiguration>
AwsParameterClientProvider::CreateClientConfiguration(
    const string& region) noexcept {
  return common::CreateClientConfiguration(make_shared<string>(move(region)));
}

ExecutionResult AwsParameterClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::Run() noexcept {
  auto region_code_or =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client_provider_);
  if (!region_code_or.Successful()) {
    SCP_ERROR(kAwsParameterClientProvider, kZeroUuid, region_code_or.result(),
              "Failed to get region code for current instance");
    return region_code_or.result();
  }

  ssm_client_ = ssm_client_factory_->CreateSSMClient(
      *CreateClientConfiguration(*region_code_or), io_async_executor_);

  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::GetParameter(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        list_parameters_context) noexcept {
  if (list_parameters_context.request->parameter_name().empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME);
    SCP_ERROR_CONTEXT(
        kAwsParameterClientProvider, list_parameters_context, execution_result,
        "Failed to get the parameter value for %s.",
        list_parameters_context.request->parameter_name().c_str());
    list_parameters_context.result = execution_result;
    list_parameters_context.Finish();
    return execution_result;
  }

  GetParametersRequest request;
  request.AddNames(list_parameters_context.request->parameter_name().c_str());

  ssm_client_->GetParametersAsync(
      request,
      bind(&AwsParameterClientProvider::OnGetParametersCallback, this,
           list_parameters_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsParameterClientProvider::OnGetParametersCallback(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        list_parameters_context,
    const Aws::SSM::SSMClient*, const GetParametersRequest&,
    const GetParametersOutcome& outcome,
    const shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    auto error_type = outcome.GetError().GetErrorType();
    list_parameters_context.result = SSMErrorConverter::ConvertSSMError(
        error_type, outcome.GetError().GetMessage());
    list_parameters_context.Finish();
    return;
  }

  if (outcome.GetResult().GetParameters().size() < 1) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kAwsParameterClientProvider, list_parameters_context, execution_result,
        "Failed to get the parameter value for %s.",
        list_parameters_context.request->parameter_name().c_str());
    list_parameters_context.result = execution_result;
    list_parameters_context.Finish();
    return;
  }

  if (outcome.GetResult().GetParameters().size() > 1) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND);
    SCP_ERROR_CONTEXT(
        kAwsParameterClientProvider, list_parameters_context, execution_result,
        "Failed to get the parameter value for %s.",
        list_parameters_context.request->parameter_name().c_str());
    list_parameters_context.result = execution_result;
    list_parameters_context.Finish();
    return;
  }

  list_parameters_context.response = make_shared<GetParameterResponse>();
  list_parameters_context.response->set_parameter_value(
      outcome.GetResult().GetParameters()[0].GetValue().c_str());
  list_parameters_context.result = SuccessExecutionResult();
  list_parameters_context.Finish();
}

shared_ptr<SSMClient> SSMClientFactory::CreateSSMClient(
    ClientConfiguration& client_config,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  client_config.executor = make_shared<AwsAsyncExecutor>(io_async_executor);
  return make_shared<SSMClient>(client_config);
}

#ifndef TEST_CPIO
shared_ptr<ParameterClientProviderInterface>
ParameterClientProviderFactory::Create(
    const shared_ptr<ParameterClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<AwsParameterClientProvider>(
      options, instance_client_provider, io_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
