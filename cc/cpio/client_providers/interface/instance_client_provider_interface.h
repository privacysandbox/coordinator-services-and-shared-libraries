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

#include "core/interface/http_client_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching instance data.
 */
class InstanceClientProviderInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Get the Current Instance Resource Name object
   *
   * @param context context of the operation.
   * @return core::ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept = 0;

  /**
   * @brief Get the Current Instance Resource  object synchronously.
   *
   * @param resource_name[out] the resource name of current instance.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept = 0;

  /**
   * @brief Get the Tags By Resource Name object
   *
   * @param context context of the operation.
   * @return core::ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept = 0;

  /**
   * @brief Get the Instance Details By Resource Name object
   *
   * @param context context of the operation.
   * @return core::ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept = 0;

  /**
   * @brief Get the Instance Details By Resource Name object synchronously.
   *
   * @param resource_name the given resource name
   * @param instance_details the details of the given instance.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult GetInstanceDetailsByResourceNameSync(
      const std::string& resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept = 0;
};

class InstanceClientProviderFactory {
 public:
  /**
   * @brief Factory to create InstanceClientProvider.
   *
   * @param http_client instance of http_client.
   * @return std::shared_ptr<InstanceClientProviderInterface>
   */
  static std::shared_ptr<InstanceClientProviderInterface> Create(
      const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
      const std::shared_ptr<core::HttpClientInterface>& http1_client,
      const std::shared_ptr<core::HttpClientInterface>& http2_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);
};
}  // namespace google::scp::cpio::client_providers
