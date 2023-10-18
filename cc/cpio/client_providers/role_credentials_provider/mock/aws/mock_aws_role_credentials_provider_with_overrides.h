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

#pragma once

#include <memory>
#include <string>

#include <aws/sts/STSClient.h>

#include "cc/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cc/cpio/client_providers/role_credentials_provider/src/aws/aws_role_credentials_provider.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"

#include "mock_aws_sts_client.h"

namespace google::scp::cpio::client_providers::mock {

class MockAwsRoleCredentialsProviderWithOverrides
    : public AwsRoleCredentialsProvider {
 public:
  MockAwsRoleCredentialsProviderWithOverrides()
      : AwsRoleCredentialsProvider(
            std::make_shared<MockInstanceClientProvider>(),
            std::make_shared<core::async_executor::mock::MockAsyncExecutor>(),
            std::make_shared<core::async_executor::mock::MockAsyncExecutor>()) {
  }

  core::ExecutionResult Run() noexcept override {
    auto execution_result = AwsRoleCredentialsProvider::Run();
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    sts_client_ = std::make_shared<MockSTSClient>();
    session_name_ = std::make_shared<std::string>("session_name");
    return core::SuccessExecutionResult();
  }

  std::shared_ptr<MockInstanceClientProvider> GetInstanceClientProvider() {
    return std::dynamic_pointer_cast<MockInstanceClientProvider>(
        instance_client_provider_);
  }

  std::shared_ptr<MockSTSClient> GetSTSClient() {
    return std::dynamic_pointer_cast<MockSTSClient>(sts_client_);
  }

  std::shared_ptr<core::AsyncExecutorInterface> GetCpuAsyncExecutor() {
    return std::dynamic_pointer_cast<core::AsyncExecutorInterface>(
        cpu_async_executor_);
  }

  void OnGetRoleCredentialsCallback(
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_credentials_context,
      const Aws::STS::STSClient* sts_client,
      const Aws::STS::Model::AssumeRoleRequest& get_credentials_request,
      const Aws::STS::Model::AssumeRoleOutcome& get_credentials_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    AwsRoleCredentialsProvider::OnGetRoleCredentialsCallback(
        get_credentials_context, sts_client, get_credentials_request,
        get_credentials_outcome, async_context);
  }
};
}  // namespace google::scp::cpio::client_providers::mock
