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
#include <utility>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "google/cloud/secretmanager/secret_manager_client.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc ParameterClientProviderInterface
 */
class GcpParameterClientProvider : public ParameterClientProviderInterface {
 public:
  /**
   * @brief Construct a new Gcp Parameter Client Provider object
   *
   * @param async_executor async executor.
   * @param io_async_executor I/O bound async executor.
   * @param instance_client_provider Gcp instance client.
   * @param options configurations for ParameterClient.
   */
  GcpParameterClientProvider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<ParameterClientOptions>& options)
      : async_executor_(async_executor),
        io_async_executor_(io_async_executor),
        instance_client_provider_(instance_client_provider) {}

  GcpParameterClientProvider() = delete;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          get_parameter_context) noexcept override;

 protected:
  /**
   * @brief Get the default Secret Manager Service Client object.
   *
   * @return std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
   */
  virtual std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
  GetSecretManagerClient() noexcept;

 private:
  /**
   * @brief Is called by async executor in order to get the secret of parameter
   * from Gcp Secret Manager.
   *
   * @param get_parameter_context the context object of the get parameter
   * operation.
   * @param access_secret_request the request Gcp Secret Manager to access
   * secret from.
   */
  void AsyncGetParameterCallback(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          get_parameter_context,
      cloud::secretmanager::v1::AccessSecretVersionRequest&
          access_secret_request) noexcept;

  /// Project ID of current instance.
  std::string project_id_;

  /// An instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the IO async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;

  /// An instance of Gcp instance client.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;

  /// An instance of the GCP Secret Manager client.
  std::shared_ptr<const cloud::secretmanager::SecretManagerServiceClient>
      sm_client_shared_;
};
}  // namespace google::scp::cpio::client_providers
