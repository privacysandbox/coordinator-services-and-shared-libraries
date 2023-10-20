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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/autoscaling/AutoScalingClient.h>

#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class AutoScalingClientFactory;

/*! @copydoc AutoScalingClientInterface
 */
class AwsAutoScalingClientProvider : public AutoScalingClientProviderInterface {
 public:
  /**
   * @brief Constructs a new Aws AutoScaling Client Provider object
   *
   * @param instance_client_provider Aws instance client.
   * @param auto_scaling_client_factory provides Aws AutoScalingClient.
   * @param io_async_executor The Aws io async executor.
   */
  AwsAutoScalingClientProvider(
      const std::shared_ptr<AutoScalingClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<AutoScalingClientFactory>&
          auto_scaling_client_factory =
              std::make_shared<AutoScalingClientFactory>())
      : instance_client_provider_(instance_client_provider),
        io_async_executor_(io_async_executor),
        auto_scaling_client_factory_(auto_scaling_client_factory) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult TryFinishInstanceTermination(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          context) noexcept override;

 private:
  /**
   * @brief Is called after AWS DescribeAutoScalingInstances call is completed.
   *
   * @param context the get auto_scaling operation
   * context.
   * @param outcome the operation outcome of AWS DescribeAutoScalingInstances.
   */
  void OnDescribeAutoScalingInstancesCallback(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>& context,
      const Aws::AutoScaling::AutoScalingClient*,
      const Aws::AutoScaling::Model::DescribeAutoScalingInstancesRequest&,
      const Aws::AutoScaling::Model::DescribeAutoScalingInstancesOutcome&
          outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) noexcept;

  /**
   * @brief Is called after AWS CompleteLifecycleAction call is completed.
   *
   * @param context the get auto_scaling operation
   * context.
   * @param outcome the operation outcome of AWS CompleteLifecycleAction.
   */
  void OnCompleteLifecycleActionCallback(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>& context,
      const Aws::AutoScaling::AutoScalingClient*,
      const Aws::AutoScaling::Model::CompleteLifecycleActionRequest&,
      const Aws::AutoScaling::Model::CompleteLifecycleActionOutcome& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) noexcept;

  /**
   * @brief Creates the Client Config object.
   *
   * @param region the region of the client.
   * @return std::shared_ptr<Aws::Client::ClientConfiguration> client
   * configuration.
   */
  virtual std::shared_ptr<Aws::Client::ClientConfiguration>
  CreateClientConfiguration(const std::string& region) noexcept;

  std::shared_ptr<AutoScalingClientOptions> options_;
  /// InstanceClientProvider.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;
  /// Instance of the io async executor
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
  /// AutoScalingClientFactory.
  std::shared_ptr<AutoScalingClientFactory> auto_scaling_client_factory_;
  /// AutoScalingClient.
  std::shared_ptr<Aws::AutoScaling::AutoScalingClient> auto_scaling_client_;
};

/// Provides AutoScalingClient.
class AutoScalingClientFactory {
 public:
  /**
   * @brief Creates AutoScalingClient.
   *
   * @param client_config the Configuratoin to create the client.
   * @return std::shared_ptr<Aws::AutoScaling::AutoScalingClient> the creation
   * result.
   */
  virtual std::shared_ptr<Aws::AutoScaling::AutoScalingClient>
  CreateAutoScalingClient(Aws::Client::ClientConfiguration& client_config,
                          const std::shared_ptr<core::AsyncExecutorInterface>&
                              io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers
