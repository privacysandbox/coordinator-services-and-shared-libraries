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

#include "gcp_parameter_client_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "google/cloud/secretmanager/secret_manager_client.h"
#include "google/cloud/secretmanager/secret_manager_connection.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"

using absl::StrFormat;
using google::cloud::StatusCode;
using google::cloud::StatusOr;
using google::cloud::secretmanager::MakeSecretManagerServiceConnection;
using google::cloud::secretmanager::SecretManagerServiceClient;
using google::cloud::secretmanager::v1::AccessSecretVersionRequest;
using google::cloud::secretmanager::v1::AccessSecretVersionResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_CREATE_SM_CLIENT_FAILURE;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::common::GcpUtils;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

static constexpr char kGcpParameterClientProvider[] =
    "GcpParameterClientProvider";
static constexpr char kGcpSecretNameFormatString[] =
    "projects/%s/secrets/%s/versions/latest";

namespace google::scp::cpio::client_providers {
ExecutionResult GcpParameterClientProvider::Init() noexcept {
  auto project_id_or =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client_provider_);
  if (!project_id_or.Successful()) {
    SCP_ERROR(kGcpParameterClientProvider, kZeroUuid, project_id_or.result(),
              "Failed to get project ID for current instance");
    return project_id_or.result();
  }
  project_id_ = move(*project_id_or);

  sm_client_shared_ = GetSecretManagerClient();
  if (!sm_client_shared_) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_PARAMETER_CLIENT_PROVIDER_CREATE_SM_CLIENT_FAILURE);
    SCP_ERROR(kGcpParameterClientProvider, kZeroUuid, execution_result,
              "Failed to create secret manager service client.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpParameterClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpParameterClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

shared_ptr<SecretManagerServiceClient>
GcpParameterClientProvider::GetSecretManagerClient() noexcept {
  return make_shared<SecretManagerServiceClient>(
      MakeSecretManagerServiceConnection());
}

ExecutionResult GcpParameterClientProvider::GetParameter(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        get_parameter_context) noexcept {
  const auto& secret = get_parameter_context.request->parameter_name();
  if (secret.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME);
    SCP_ERROR_CONTEXT(kGcpParameterClientProvider, get_parameter_context,
                      execution_result, "Failed due to an empty parameter.");
    get_parameter_context.result = execution_result;
    get_parameter_context.Finish();
    return execution_result;
  }

  string name = StrFormat(kGcpSecretNameFormatString, project_id_, secret);

  AccessSecretVersionRequest access_secret_request;
  access_secret_request.set_name(name);

  auto schedule_result = io_async_executor_->Schedule(
      bind(&GcpParameterClientProvider::AsyncGetParameterCallback, this,
           get_parameter_context, move(access_secret_request)),
      AsyncPriority::Normal);

  if (!schedule_result.Successful()) {
    get_parameter_context.result = schedule_result;
    get_parameter_context.Finish();

    SCP_ERROR_CONTEXT(kGcpParameterClientProvider, get_parameter_context,
                      schedule_result,
                      "Failed to schedule AsyncGetParameterCallback().");
    return schedule_result;
  }

  return SuccessExecutionResult();
}

void GcpParameterClientProvider::AsyncGetParameterCallback(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        get_parameter_context,
    AccessSecretVersionRequest& access_secret_request) noexcept {
  SecretManagerServiceClient client(*sm_client_shared_);

  auto secret_result = client.AccessSecretVersion(access_secret_request);
  auto result = GcpUtils::GcpErrorConverter(secret_result.status());

  if (!result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpParameterClientProvider, get_parameter_context,
                      result, "Failed to get parameter with %s",
                      secret_result.status().message().c_str());

    get_parameter_context.result = result;
    FinishContext(result, get_parameter_context, async_executor_);
    return;
  }

  auto version = move(secret_result).value();
  get_parameter_context.response = make_shared<GetParameterResponse>();
  get_parameter_context.response->set_parameter_value(version.payload().data());
  get_parameter_context.result = SuccessExecutionResult();

  FinishContext(SuccessExecutionResult(), get_parameter_context,
                async_executor_);
}

#ifndef TEST_CPIO
shared_ptr<ParameterClientProviderInterface>
ParameterClientProviderFactory::Create(
    const shared_ptr<ParameterClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpParameterClientProvider>(
      cpu_async_executor, io_async_executor, instance_client_provider, options);
}
#endif
}  // namespace google::scp::cpio::client_providers
