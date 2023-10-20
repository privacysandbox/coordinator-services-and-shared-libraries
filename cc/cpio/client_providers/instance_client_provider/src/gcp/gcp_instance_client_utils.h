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

#include "core/interface/http_types.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {

/// Represents the details of instance resource name.
struct GcpInstanceResourceNameDetails {
  std::string project_id;
  std::string zone_id;
  std::string instance_id;
};

class GcpInstanceClientUtils {
 public:
  /**
   * @brief Get the Current Project Id object from instance client.
   *
   * @param instance_client the instance client instance used to retrieve the
   * project ID to which the current instance belongs.
   * @return core::ExecutionResultOr<std::string> project ID if success.
   */
  static core::ExecutionResultOr<std::string> GetCurrentProjectId(
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client) noexcept;

  /**
   * @brief Parse the project ID from the Instance Resource Name.
   *
   * @param resource_name Instance resource name.
   * @return core::ExecutionResultOr<std::string> project ID if success.
   */
  static core::ExecutionResultOr<std::string>
  ParseProjectIdFromInstanceResourceName(
      const std::string& resource_name) noexcept;

  /**
   * @brief Parse the project ID from the Instance Resource Name.
   *
   * @param resource_name Instance resource name.
   * @param zone_id[out] zone id which the instance belong
   * @return core::ExecutionResultOr<std::string> zone ID if success.
   */
  static core::ExecutionResultOr<std::string>
  ParseZoneIdFromInstanceResourceName(
      const std::string& resource_name) noexcept;

  /**
   * @brief Parse the instance ID from the Instance Resource Name.
   *
   * @param resource_name Instance resource name.
   * @param instance_id[out] instance ID.
   * @return core::ExecutionResultOr<std::string> instance ID if success.
   */
  static core::ExecutionResultOr<std::string>
  ParseInstanceIdFromInstanceResourceName(
      const std::string& resource_name) noexcept;

  /**
   * @brief Validate instance resource name.
   *
   * @param resource_name Instance resource name.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ValidateInstanceResourceNameFormat(
      const std::string& resource_name) noexcept;

  /**
   * @brief Get the Instance Resource Id Details object
   *
   * @param resource_name Instance resource name.
   * @param details[out] the details of instance resource name.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetInstanceResourceNameDetails(
      const std::string& resource_name,
      GcpInstanceResourceNameDetails& details) noexcept;

  /**
   * @brief Create the resource manager url for listing all tags attached to a
   * resource
   *
   * @param resource_name full resource name of a resource.
   * @return std::string the url for listing all tags attached to a resource.
   */
  static std::string CreateRMListTagsUrl(
      const std::string& resource_name) noexcept;
};
}  // namespace google::scp::cpio::client_providers
