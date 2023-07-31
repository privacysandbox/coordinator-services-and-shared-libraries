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

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/config_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc ConfigClientProviderInterface
 */
class ConfigClientProvider : public ConfigClientProviderInterface {
 public:
  virtual ~ConfigClientProvider() = default;

  explicit ConfigClientProvider(
      const std::shared_ptr<ConfigClientOptions>& config_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetTag(
      core::AsyncContext<config_client::GetTagProtoRequest,
                         config_client::GetTagProtoResponse>& context) noexcept
      override;

  core::ExecutionResult GetInstanceId(
      core::AsyncContext<config_client::GetInstanceIdProtoRequest,
                         config_client::GetInstanceIdProtoResponse>&
          context) noexcept override;

  core::ExecutionResult GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          context) noexcept override;

 protected:
  /**
   * @brief To be called when get parameter finished.
   *
   * @param config_client_context context to get parameter from ConfigClient.
   * @param parameter_client_context context to get parameter from
   * ParameterClient.
   * @return core::ExecutionResult the fetch results.
   */
  void OnGetParameterCallback(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          config_client_context,
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          parameter_client_context) noexcept;

  /// InstanceClientProvider.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;

  /// ParameterClientProvider.
  std::shared_ptr<ParameterClientProviderInterface> parameter_client_provider_;
};
}  // namespace google::scp::cpio::client_providers
