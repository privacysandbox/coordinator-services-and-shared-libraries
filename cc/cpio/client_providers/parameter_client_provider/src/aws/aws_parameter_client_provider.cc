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
#include "core/interface/async_context.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
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
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
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
ExecutionResult AwsParameterClientProvider::CreateClientConfiguration(
    shared_ptr<ClientConfiguration>& client_config) noexcept {
  auto region = make_shared<string>();
  auto execution_result =
      instance_client_provider_->GetCurrentInstanceRegion(*region);
  if (!execution_result.Successful()) {
    ERROR(kAwsParameterClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get region");
    return execution_result;
  }

  client_config = common::CreateClientConfiguration(region);
  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::Init() noexcept {
  shared_ptr<ClientConfiguration> client_config;
  auto execution_result = CreateClientConfiguration(client_config);
  if (!execution_result.Successful()) {
    ERROR(kAwsParameterClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to create ClientConfiguration");
    return execution_result;
  }

  ssm_client_ = make_shared<SSMClient>(*client_config);

  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::Run() noexcept {
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
    ERROR_CONTEXT(kAwsParameterClientProvider, list_parameters_context,
                  execution_result, "Failed to get the parameter value for %s.",
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
    ERROR_CONTEXT(kAwsParameterClientProvider, list_parameters_context,
                  execution_result, "Failed to get the parameter value for %s.",
                  list_parameters_context.request->parameter_name().c_str());
    list_parameters_context.result = execution_result;
    list_parameters_context.Finish();
    return;
  }

  if (outcome.GetResult().GetParameters().size() > 1) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND);
    ERROR_CONTEXT(kAwsParameterClientProvider, list_parameters_context,
                  execution_result, "Failed to get the parameter value for %s.",
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
  return;
}

#ifndef TEST_CPIO
std::shared_ptr<ParameterClientProviderInterface>
ParameterClientProviderFactory::Create(
    const shared_ptr<ParameterClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>&
        instance_client_provider) {
  return make_shared<AwsParameterClientProvider>(options,
                                                 instance_client_provider);
}
#endif
}  // namespace google::scp::cpio::client_providers
