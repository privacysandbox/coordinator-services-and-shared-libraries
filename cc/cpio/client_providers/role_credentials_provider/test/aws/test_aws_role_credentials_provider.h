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

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "cpio/client_providers/role_credentials_provider/src/aws/aws_role_credentials_provider.h"
#include "public/cpio/test/test_aws_role_credentials_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AwsRoleCredentialsProvider
 */
class TestAwsRoleCredentialsProvider : public AwsRoleCredentialsProvider {
 public:
  explicit TestAwsRoleCredentialsProvider(
      const std::shared_ptr<TestAwsRoleCredentialsOptions>&
          role_credentials_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor)
      : AwsRoleCredentialsProvider(role_credentials_options,
                                   instance_client_provider, async_executor),
        cloud_watch_endpoint_override_(
            role_credentials_options->cloud_watch_endpoint_override),
        region_(role_credentials_options->region) {}

 protected:
  core::ExecutionResult CreateClientConfiguration(
      std::shared_ptr<Aws::Client::ClientConfiguration>& client_config) noexcept
      override;

  std::shared_ptr<std::string> cloud_watch_endpoint_override_;
  std::shared_ptr<std::string> region_;
};
}  // namespace google::scp::cpio::client_providers
