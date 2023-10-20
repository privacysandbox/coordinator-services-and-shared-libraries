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

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/parameter_client/type_def.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching parameters from cloud.
 */
class ParameterClientProviderInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Fetches the parameter value.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          context) noexcept = 0;
};

class ParameterClientProviderFactory {
 public:
  /**
   * @brief Factory to create ParameterClientProvider.
   *
   * @param instance_client_provider InstanceClientProvider.
   * @return std::shared_ptr<ParameterClientProviderInterface> created
   * ParameterClientProvider.
   */
  static std::shared_ptr<ParameterClientProviderInterface> Create(
      const std::shared_ptr<ParameterClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);
};
}  // namespace google::scp::cpio::client_providers
