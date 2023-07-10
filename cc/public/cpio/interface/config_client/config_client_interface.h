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

#ifndef SCP_CPIO_INTERFACE_CONFIG_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_CONFIG_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"

#include "type_def.h"

namespace google::scp::cpio {
/// Request for GetParameter.
struct GetParameterRequest {
  /**
   * @brief It should be one of the list of parameter_names passed in the
   * option, otherwise, not found error will be returned.
   */
  ParameterName parameter_name;
};

/// Response for GetParameter.
struct GetParameterResponse {
  /// The value of the parameter for the given name.
  ParameterValue parameter_value;
};

/// Request for GetTag.
struct GetTagRequest {
  /**
   * @brief It should be one of the list of tag_names passed in the
   * option, otherwise, not found error will be returned.
   */
  TagName tag_name;
};

/// Response for GetTag.
struct GetTagResponse {
  /// The value of the tag for the given name.
  TagValue tag_value;
};

/// Request for GetInstanceId.
struct GetInstanceIdRequest {};

/// Response for GetInstanceId.
struct GetInstanceIdResponse {
  /// The instance ID the application is running on.
  InstanceId instance_id;
};

/**
 * @brief Interface responsible for fetching pre-stored application data or
 * cloud metadata.
 *
 * Use ConfigClientFactory::Create to create the ConfigClient. Call
 * ConfigClientInterface::Init and ConfigClientInterface::Run before actually
 * use it, and call ConfigClientInterface::Stop when finish using it.
 */
class ConfigClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Gets parameter value for a given name.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetParameter(
      GetParameterRequest request,
      Callback<GetParameterResponse> callback) noexcept = 0;

  /**
   * @brief Gets tag value.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetTag(
      GetTagRequest request, Callback<GetTagResponse> callback) noexcept = 0;

  /**
   * @brief Gets the instance ID the code is running on.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetInstanceId(
      GetInstanceIdRequest request,
      Callback<GetInstanceIdResponse> callback) noexcept = 0;
};

/// Factory to create ConfigClient.
class ConfigClientFactory {
 public:
  /**
   * @brief Creates ConfigClient.
   *
   * @param options configurations for ConfigClient.
   * @return std::unique_ptr<ConfigClientInterface> ConfigClient object.
   */
  static std::unique_ptr<ConfigClientInterface> Create(
      ConfigClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_CONFIG_CLIENT_INTERFACE_H_
