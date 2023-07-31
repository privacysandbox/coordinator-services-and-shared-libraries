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

/// Represents the details of AWS instance resource name.
struct AwsResourceNameDetails {
  std::string account_id;
  std::string region;
  std::string resource_id;
};

class AwsInstanceClientUtils {
 public:
  /**
   * @brief Get the Current aws region code object from instance client.
   *
   * @param instance_client the instance client instance used to retrieve the
   * region code to which the current instance belongs.
   * @return core::ExecutionResultOr<std::string> AWS region code if success.
   */
  static core::ExecutionResultOr<std::string> GetCurrentRegionCode(
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client) noexcept;

  /**
   * @brief Parse region from the Instance Resource Name.
   *
   * @param resource_name Instance resource name.
   * @return core::ExecutionResultOr<std::string> region
   */
  static core::ExecutionResultOr<std::string> ParseRegionFromResourceName(
      const std::string& resource_name) noexcept;

  /**
   * @brief Parse the project ID from the Instance Resource Name.
   *
   * @param resource_name Instance resource name.
   * @return core::ExecutionResultOr<std::string> account id.
   */
  static core::ExecutionResultOr<std::string> ParseAccountIdFromResourceName(
      const std::string& resource_name) noexcept;

  /**
   * @brief Parse the instance ID from the Instance Resource Name.
   *
   * @param resource_name Instance resource name.
   * @return core::ExecutionResultOr<std::string> resource ID
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
  static core::ExecutionResult ValidateResourceNameFormat(
      const std::string& resource_name) noexcept;

  /**
   * @brief Get the Instance Resource Id Details object
   *
   * @param resource_name Instance resource name.
   * @param details[out] the details of instance resource name.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetResourceNameDetails(
      const std::string& resource_name,
      AwsResourceNameDetails& details) noexcept;
};
}  // namespace google::scp::cpio::client_providers
