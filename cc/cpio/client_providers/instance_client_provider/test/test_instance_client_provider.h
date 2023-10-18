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

#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/test/global_cpio/test_cpio_options.h"

namespace google::scp::cpio::client_providers {
/// Configurations for Test InstanceClientProvider.
struct TestInstanceClientOptions {
  TestInstanceClientOptions() = default;

  explicit TestInstanceClientOptions(const TestCpioOptions& cpio_options)
      : region(cpio_options.region),
        instance_id(cpio_options.instance_id),
        public_ipv4_address(cpio_options.public_ipv4_address),
        private_ipv4_address(cpio_options.private_ipv4_address),
        owner_id(cpio_options.owner_id),
        zone(cpio_options.zone) {}

  /// Cloud region.
  std::string region;
  /// Instance ID.
  std::string instance_id;
  /// Public IP address.
  std::string public_ipv4_address;
  /// Private IP address.
  std::string private_ipv4_address;
  /// Owner ID.
  std::string owner_id;
  /// Zone ID.
  std::string zone;
};

/**
 * @copydoc InstanceClientProviderInterface.
 */
class TestInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  explicit TestInstanceClientProvider(
      const std::shared_ptr<TestInstanceClientOptions>& test_options)
      : test_options_(test_options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept override;

  core::ExecutionResult GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept override;

  core::ExecutionResult GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept override;

  core::ExecutionResult GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override;

  core::ExecutionResult GetInstanceDetailsByResourceNameSync(
      const std::string& resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept override;

 protected:
  std::shared_ptr<TestInstanceClientOptions> test_options_;
};
}  // namespace google::scp::cpio::client_providers
