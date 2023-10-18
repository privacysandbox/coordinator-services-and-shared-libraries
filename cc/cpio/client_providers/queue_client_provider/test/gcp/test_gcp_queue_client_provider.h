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

#include "cpio/client_providers/queue_client_provider/src/gcp/gcp_queue_client_provider.h"

namespace google::scp::cpio::client_providers {
/// QueueClientOptions for testing on GCP.
struct TestGcpQueueClientOptions : public QueueClientOptions {
  std::shared_ptr<std::string> pubsub_client_endpoint_override =
      std::make_shared<std::string>();
};

/*! @copydoc GcpPubSubStubFactory
 */
class TestGcpPubSubStubFactory : public GcpPubSubStubFactory {
 private:
  std::shared_ptr<grpc::Channel> GetPubSubChannel(
      const std::shared_ptr<QueueClientOptions>& options) noexcept override;
};

/*! @copydoc GcpQueueClientProvider
 */
class TestGcpQueueClientProvider : public GcpQueueClientProvider {
 public:
  explicit TestGcpQueueClientProvider(
      const std::shared_ptr<TestGcpQueueClientOptions>& queue_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor)
      : GcpQueueClientProvider(queue_client_options, instance_client_provider,
                               cpu_async_executor, io_async_executor,
                               std::make_shared<TestGcpPubSubStubFactory>()) {}
};
}  // namespace google::scp::cpio::client_providers
