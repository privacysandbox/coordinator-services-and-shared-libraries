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

#include "cpio/client_providers/metric_client_provider/src/aws/aws_metric_client_provider.h"
#include "public/cpio/test/metric_client/test_aws_metric_client_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AwsMetricClientProvider
 */
class TestAwsMetricClientProvider : public AwsMetricClientProvider {
 public:
  explicit TestAwsMetricClientProvider(
      const std::shared_ptr<TestAwsMetricClientOptions>& metric_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<MetricBatchingOptions>& metric_batching_options =
          std::make_shared<MetricBatchingOptions>())
      : AwsMetricClientProvider(metric_client_options, instance_client_provider,
                                async_executor, io_async_executor,
                                metric_batching_options),
        cloud_watch_endpoint_override_(
            metric_client_options->cloud_watch_endpoint_override) {}

 protected:
  void CreateClientConfiguration(
      const std::shared_ptr<std::string>& region,
      std::shared_ptr<Aws::Client::ClientConfiguration>& client_config) noexcept
      override;

  std::shared_ptr<std::string> cloud_watch_endpoint_override_;
};
}  // namespace google::scp::cpio::client_providers
