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

#include "cpio/client_providers/kms_client_provider/src/aws/nontee_aws_kms_client_provider.h"
#include "public/cpio/test/kms_client/test_aws_kms_client_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AwsKmsClientProvider
 */
class TestAwsKmsClientProvider : public NonteeAwsKmsClientProvider {
 public:
  explicit TestAwsKmsClientProvider(
      const std::shared_ptr<TestAwsKmsClientOptions>& options,
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor)
      : NonteeAwsKmsClientProvider(role_credentials_provider,
                                   io_async_executor),
        test_options_(options) {}

 protected:
  std::shared_ptr<Aws::Client::ClientConfiguration> CreateClientConfiguration(
      const std::string& region) noexcept override;

  std::shared_ptr<TestAwsKmsClientOptions> test_options_;
};
}  // namespace google::scp::cpio::client_providers
