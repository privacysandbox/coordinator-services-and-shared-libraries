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

#include "test_gcp_queue_client_provider.h"

#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "cpio/client_providers/interface/queue_client_provider_interface.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using grpc::Channel;
using grpc::ChannelArguments;
using grpc::CreateCustomChannel;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
shared_ptr<Channel> TestGcpPubSubStubFactory::GetPubSubChannel(
    const shared_ptr<QueueClientOptions>& options) noexcept {
  if (!channel_) {
    ChannelArguments args;
    args.SetInt(GRPC_ARG_ENABLE_RETRIES, 1);  // enable
    channel_ = CreateCustomChannel(
        *dynamic_pointer_cast<TestGcpQueueClientOptions>(options)
             ->pubsub_client_endpoint_override,
        grpc::InsecureChannelCredentials(), args);
  }
  return channel_;
}

shared_ptr<QueueClientProviderInterface> QueueClientProviderFactory::Create(
    const shared_ptr<QueueClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface> instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  return make_shared<TestGcpQueueClientProvider>(
      std::dynamic_pointer_cast<TestGcpQueueClientOptions>(options),
      instance_client_provider, cpu_async_executor, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
