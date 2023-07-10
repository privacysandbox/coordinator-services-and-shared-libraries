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
#include <utility>
#include <vector>

#include "cpio/client_providers/interface/instance_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  std::string instance_id_mock = "instance_id";
  core::ExecutionResult get_instance_id_result_mock =
      core::SuccessExecutionResult();

  core::ExecutionResult GetCurrentInstanceId(
      std::string& instance_id) noexcept override {
    if (get_instance_id_result_mock != core::SuccessExecutionResult()) {
      return get_instance_id_result_mock;
    }
    instance_id = instance_id_mock;
    return core::SuccessExecutionResult();
  }

  std::string region_mock = "us-east-1";
  core::ExecutionResult get_region_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult GetCurrentInstanceRegion(
      std::string& region) noexcept override {
    if (get_region_result_mock != core::SuccessExecutionResult()) {
      return get_region_result_mock;
    }
    region = region_mock;
    return core::SuccessExecutionResult();
  }

  std::map<std::string, std::string> tag_values_mock = {
      std::make_pair("tag1", "value1")};
  core::ExecutionResult get_tags_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult GetTagsOfInstance(
      const std::vector<std::string>& tag_names, const std::string& instance_id,
      std::map<std::string, std::string>& tag_values_map) noexcept override {
    if (get_tags_result_mock != core::SuccessExecutionResult()) {
      return get_tags_result_mock;
    }
    tag_values_map = tag_values_mock;
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult GetCurrentInstancePublicIpv4Address(
      std::string& instance_public_ipv4_address) noexcept override {
    instance_public_ipv4_address = "1.1.1.1";
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult GetCurrentInstancePrivateIpv4Address(
      std::string& instance_private_ipv4_address) noexcept override {
    instance_private_ipv4_address = "10.1.1.1";
    return core::SuccessExecutionResult();
  }

  std::string project_id_mock = "12345";
  core::ExecutionResult get_project_id_result_mock =
      core::SuccessExecutionResult();

  core::ExecutionResult GetCurrentInstanceProjectId(
      std::string& project_id) noexcept override {
    if (get_project_id_result_mock != core::SuccessExecutionResult()) {
      return get_project_id_result_mock;
    }
    project_id = project_id_mock;
    return core::SuccessExecutionResult();
  }

  std::string instance_zone_mock = "zone-a";
  core::ExecutionResult get_instance_zone_result_mock =
      core::SuccessExecutionResult();

  core::ExecutionResult GetCurrentInstanceZone(
      std::string& instance_zone) noexcept override {
    if (get_instance_zone_result_mock != core::SuccessExecutionResult()) {
      return get_instance_zone_result_mock;
    }
    instance_zone = instance_zone_mock;
    return core::SuccessExecutionResult();
  }

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
};
}  // namespace google::scp::cpio::client_providers::mock
