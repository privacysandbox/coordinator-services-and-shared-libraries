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

#ifndef SCP_CPIO_INTERFACE_INSTANCE_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_INSTANCE_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for fetching cloud instance metadata.
 *
 * Use InstanceClientFactory::Create to create the InstanceClient. Call
 * InstanceClientInterface::Init and InstanceClientInterface::Run before
 * actually use it, and call InstanceClientInterface::Stop when finish using it.
 */
class InstanceClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Gets the resource name for the instance where the code is running
   * on.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetCurrentInstanceResourceName(
      cmrt::sdk::instance_service::v1::GetCurrentInstanceResourceNameRequest
          request,
      Callback<cmrt::sdk::instance_service::v1::
                   GetCurrentInstanceResourceNameResponse>
          callback) noexcept = 0;

  /**
   * @brief Gets all tags for the give resource.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetTagsByResourceName(
      cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest request,
      Callback<cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>
          callback) noexcept = 0;

  /**
   * @brief Gets instance details for a given instance resource name.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetInstanceDetailsByResourceName(
      cmrt::sdk::instance_service::v1::GetInstanceDetailsByResourceNameRequest
          request,
      Callback<cmrt::sdk::instance_service::v1::
                   GetInstanceDetailsByResourceNameResponse>
          callback) noexcept = 0;
};

/// Factory to create InstanceClient.
class InstanceClientFactory {
 public:
  /**
   * @brief Creates InstanceClient.
   *
   * @param options configurations for InstanceClient.
   * @return std::unique_ptr<InstanceClientInterface> InstanceClient object.
   */
  static std::unique_ptr<InstanceClientInterface> Create(
      InstanceClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_INSTANCE_CLIENT_INTERFACE_H_
