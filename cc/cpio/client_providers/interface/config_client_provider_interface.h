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
#include "cpio/proto/config_client.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/config_client/type_def.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching worker parameters from storage.
 */
class ConfigClientProviderInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Fetches the value of a parameter.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          context) noexcept = 0;

  /**
   * @brief Fetches the tag.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetTag(
      core::AsyncContext<config_client::GetTagProtoRequest,
                         config_client::GetTagProtoResponse>&
          context) noexcept = 0;

  /**
   * @brief Fetches the instance ID.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetInstanceId(
      core::AsyncContext<config_client::GetInstanceIdProtoRequest,
                         config_client::GetInstanceIdProtoResponse>&
          context) noexcept = 0;
};

class ConfigClientProviderFactory {
 public:
  /**
   * @brief Factory to create ConfigClientProvider.
   *
   * @return std::shared_ptr<ConfigClientProviderInterface> created
   * ConfigClientProvider.
   */
  static std::shared_ptr<ConfigClientProviderInterface> Create(
      const std::shared_ptr<ConfigClientOptions>& options);
};
}  // namespace google::scp::cpio::client_providers
