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

#include "test_aws_metric_client_provider.h"

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/common/test/aws/test_aws_utils.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/test/metric_client/test_aws_metric_client_options.h"

using Aws::Client::ClientConfiguration;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::common::test::CreateTestClientConfiguration;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
void TestAwsMetricClientProvider::CreateClientConfiguration(
    const shared_ptr<string>& region,
    shared_ptr<ClientConfiguration>& client_config) noexcept {
  client_config =
      CreateTestClientConfiguration(cloud_watch_endpoint_override_, region);
}

shared_ptr<MetricClientInterface> MetricClientProviderFactory::Create(
    const shared_ptr<MetricClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<TestAwsMetricClientProvider>(
      std::dynamic_pointer_cast<TestAwsMetricClientOptions>(options),
      instance_client_provider, async_executor, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
