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

#include "test_aws_auto_scaling_client_provider.h"

#include <memory>
#include <string>

#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/common/test/aws/test_aws_utils.h"

using Aws::Client::ClientConfiguration;
using google::scp::cpio::common::test::CreateTestClientConfiguration;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
shared_ptr<ClientConfiguration>
TestAwsAutoScalingClientProvider::CreateClientConfiguration(
    const string& region) noexcept {
  return CreateTestClientConfiguration(
      test_options_->auto_scaling_client_endpoint_override,
      make_shared<string>(region));
}

shared_ptr<AutoScalingClientProviderInterface>
AutoScalingClientProviderFactory::Create(
    const shared_ptr<AutoScalingClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<TestAwsAutoScalingClientProvider>(
      std::dynamic_pointer_cast<TestAwsAutoScalingClientOptions>(options),
      instance_client_provider, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
