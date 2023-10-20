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

#include "core/interface/async_executor_interface.h"
#include "core/interface/credentials_provider_interface.h"

namespace google::scp::core {

class AwsAssumeRoleCredentialsProvider : public CredentialsProviderInterface {
 public:
  AwsAssumeRoleCredentialsProvider(
      const std::shared_ptr<std::string>& assume_role_arn,
      const std::shared_ptr<std::string>& assume_role_external_id,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<std::string>& region)
      : assume_role_arn_(assume_role_arn),
        assume_role_external_id_(assume_role_external_id),
        async_executor_(async_executor),
        io_async_executor_(io_async_executor),
        region_(region) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult GetCredentials(
      AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
          get_credentials_context) noexcept override;

 protected:
  /**
   * @brief Is called when the get credentials operation is completed.
   *
   * @param get_credentials_context The context of the get credentials
   * operation.
   * @param sts_client The sts client.
   * @param get_credentials_request The get credentials operation request
   * object.
   * @param get_credentials_outcome The get credentials operation outcome.
   * @param async_context The async context of the operation.
   */
  virtual void OnGetCredentialsCallback(
      AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
          get_credentials_context,
      const Aws::STS::STSClient* sts_client,
      const Aws::STS::Model::AssumeRoleRequest& get_credentials_request,
      const Aws::STS::Model::AssumeRoleOutcome& get_credentials_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /// The assume role name to execute the operation.
  const std::shared_ptr<std::string> assume_role_arn_;

  /// The assume role external id to execute the operation.
  const std::shared_ptr<std::string> assume_role_external_id_;

  /// An instance of the async executor. To execute call
  const std::shared_ptr<AsyncExecutorInterface> async_executor_;

  /// An instance of the IO async executor.
  const std::shared_ptr<AsyncExecutorInterface> io_async_executor_;

  /// The AWS region of the AWS Client.
  const std::shared_ptr<std::string> region_;

  /// An instance of the AWS client configuration.
  std::shared_ptr<Aws::Client::ClientConfiguration> client_config_;

  /// An instance of the AWS STS client.
  std::shared_ptr<Aws::STS::STSClient> sts_client_;

  /// The session id
  std::shared_ptr<std::string> session_name_;
};
}  // namespace google::scp::core
