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

#include "core/credentials_provider/src/aws_assume_role_credentials_provider.h"
#include "core/interface/credentials_provider_interface.h"

namespace google::scp::core::credentials_provider::mock {

class MockAwsAssumeRoleCredentialsProvider
    : public AwsAssumeRoleCredentialsProvider {
 public:
  MockAwsAssumeRoleCredentialsProvider(
      std::shared_ptr<Aws::STS::STSClient>& sts_client,
      const std::shared_ptr<std::string>& assume_role_arn,
      const std::shared_ptr<std::string>& assume_role_external_id,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<std::string>& region)
      : AwsAssumeRoleCredentialsProvider(
            assume_role_arn, assume_role_external_id, async_executor,
            io_async_executor, region) {
    sts_client_ = sts_client;
    session_name_ = std::make_shared<std::string>("session_name");
  }

  void OnGetCredentialsCallback(
      AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
          get_credentials_context,
      const Aws::STS::STSClient* sts_client,
      const Aws::STS::Model::AssumeRoleRequest& get_credentials_request,
      const Aws::STS::Model::AssumeRoleOutcome& get_credentials_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    AwsAssumeRoleCredentialsProvider::OnGetCredentialsCallback(
        get_credentials_context, sts_client, get_credentials_request,
        get_credentials_outcome, async_context);
  }
};
}  // namespace google::scp::core::credentials_provider::mock
