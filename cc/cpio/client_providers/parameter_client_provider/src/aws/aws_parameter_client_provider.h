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

#include <aws/ssm/SSMClient.h>

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class SSMClientFactory;

/*! @copydoc ParameterClientInterface
 */
class AwsParameterClientProvider : public ParameterClientProviderInterface {
 public:
  /**
   * @brief Constructs a new Aws Parameter Client Provider object
   *
   * @param parameter_client_options configurations for ParameterClient.
   * @param instance_client_provider Aws instance client.
   * @param io_async_executor The Aws io async context.
   */
  AwsParameterClientProvider(
      const std::shared_ptr<ParameterClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<SSMClientFactory>& ssm_client_factory =
          std::make_shared<SSMClientFactory>())
      : instance_client_provider_(instance_client_provider),
        io_async_executor_(io_async_executor),
        ssm_client_factory_(ssm_client_factory) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          context) noexcept override;

 protected:
  /**
   * @brief Is called after AWS GetParameters call is completed.
   *
   * @param get_parameter_context the get parameter operation
   * context.
   * @param outcome the operation outcome of AWS GetParameters.
   */
  virtual void OnGetParametersCallback(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          get_parameter_context,
      const Aws::SSM::SSMClient*, const Aws::SSM::Model::GetParametersRequest&,
      const Aws::SSM::Model::GetParametersOutcome& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) noexcept;

  /**
   * @brief Creates a Client Configuration object.
   *
   * @param client_config returned Client Configuration.
   * @return core::ExecutionResult creation result.
   */
  virtual std::shared_ptr<Aws::Client::ClientConfiguration>
  CreateClientConfiguration(const std::string& region) noexcept;

  /// InstanceClientProvider.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;
  /// Instance of the io async executor
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
  /// SSMClient.
  std::shared_ptr<Aws::SSM::SSMClient> ssm_client_;
  std::shared_ptr<SSMClientFactory> ssm_client_factory_;
};

/// Provides SSMClient.
class SSMClientFactory {
 public:
  /**
   * @brief Creates SSMClient.
   *
   * @param client_config the Configuration to create the client.
   * @return std::shared_ptr<Aws::SSM::SSMClient> the creation
   * result.
   */
  virtual std::shared_ptr<Aws::SSM::SSMClient> CreateSSMClient(
      Aws::Client::ClientConfiguration& client_config,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept;

  virtual ~SSMClientFactory() = default;
};
}  // namespace google::scp::cpio::client_providers
