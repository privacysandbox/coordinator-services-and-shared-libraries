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

#include "test_aws_kms_client_provider.h"

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "cpio/client_providers/kms_client_provider/src/aws/nontee_aws_kms_client_provider.h"
#include "cpio/common/test/aws/test_aws_utils.h"
#include "public/cpio/interface/kms_client/type_def.h"
#include "public/cpio/test/kms_client/test_aws_kms_client_options.h"

using Aws::Client::ClientConfiguration;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::common::test::CreateTestClientConfiguration;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
shared_ptr<ClientConfiguration>
TestAwsKmsClientProvider::CreateClientConfiguration(
    const string& region) noexcept {
  return CreateTestClientConfiguration(test_options_->kms_endpoint_override,
                                       make_shared<string>(region));
}

shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<KmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const std::shared_ptr<core::AsyncExecutorInterface>&
        io_async_executor) noexcept {
  return make_shared<TestAwsKmsClientProvider>(
      dynamic_pointer_cast<TestAwsKmsClientOptions>(options),
      role_credentials_provider, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
