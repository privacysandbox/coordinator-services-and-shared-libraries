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

#include <aws/core/internal/AWSHttpResourceClient.h>
#include <aws/ec2/EC2Client.h>

#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc InstanceClientProviderInterface
 */
class AwsInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  /// Constructs a new Aws Instance Client Provider object
  AwsInstanceClientProvider();

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetCurrentInstanceId(
      std::string& instance_id) noexcept override;

  core::ExecutionResult GetCurrentInstanceRegion(
      std::string& region) noexcept override;

  core::ExecutionResult GetTagsOfInstance(
      const std::vector<std::string>& tag_names, const std::string& instance_id,
      std::map<std::string, std::string>& tag_values_map) noexcept override;

  core::ExecutionResult GetCurrentInstancePublicIpv4Address(
      std::string& instance_public_ipv4_address) noexcept override;

  core::ExecutionResult GetCurrentInstancePrivateIpv4Address(
      std::string& instance_private_ipv4_address) noexcept override;

  core::ExecutionResult GetCurrentInstanceProjectId(
      std::string&) noexcept override;

  core::ExecutionResult GetCurrentInstanceZone(std::string&) noexcept override;

  core::ExecutionResult GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult GetInstanceDetailsByResourceNameSync(
      const std::string& resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

 protected:
  /**
   * @brief Get the Resource value for the given resource name.
   *
   * @param resource_value returned resource value.
   * @param resource_name the given resource name.
   * @return ExecutionResult execution result.
   */
  core::ExecutionResult GetResource(std::string& resource_value,
                                    const std::string& resource_name) noexcept;

  /// EC2Client.
  std::shared_ptr<Aws::EC2::EC2Client> ec2_client_;
  /// EC2MetadataClient.
  std::shared_ptr<Aws::Internal::EC2MetadataClient> ec2_metadata_client_;
};
}  // namespace google::scp::cpio::client_providers
