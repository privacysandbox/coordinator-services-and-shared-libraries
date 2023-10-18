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
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {

class AwsRoleCredentialsProvider : public RoleCredentialsProviderInterface {
 public:
  AwsRoleCredentialsProvider(
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor)
      : instance_client_provider_(instance_client_provider),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetRoleCredentials(
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_credentials_context) noexcept override;

 protected:
  /**
   * @brief Is called when the get role credentials operation is completed.
   *
   * @param get_credentials_context The context of the get role credentials
   * operation.
   * @param sts_client The sts client.
   * @param get_credentials_request The get credentials operation request
   * object.
   * @param get_credentials_outcome The get credentials operation outcome.
   * @param async_context The async context of the operation.
   */
  virtual void OnGetRoleCredentialsCallback(
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_role_credentials_context,
      const Aws::STS::STSClient* sts_client,
      const Aws::STS::Model::AssumeRoleRequest& get_credentials_request,
      const Aws::STS::Model::AssumeRoleOutcome& get_credentials_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Creates the Client Config object.
   *
   * @param region the region of the client.
   * @return std::shared_ptr<Aws::Client::ClientConfiguration> client
   * configuration.
   */
  virtual std::shared_ptr<Aws::Client::ClientConfiguration>
  CreateClientConfiguration(const std::string& region) noexcept;

  /// Instance client provider to fetch cloud metadata.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;

  /// Instances of the async executor to execute call.
  const std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  /// An instance of the AWS STS client.
  std::shared_ptr<Aws::STS::STSClient> sts_client_;

  /// The session id.
  std::shared_ptr<std::string> session_name_;
};
}  // namespace google::scp::cpio::client_providers
