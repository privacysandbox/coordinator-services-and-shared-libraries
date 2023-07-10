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

#include "test_instance_client_provider.h"

#include <map>
#include <string>
#include <vector>

#include "public/core/interface/execution_result.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::map;
using std::string;
using std::vector;

namespace google::scp::cpio::client_providers {
ExecutionResult TestInstanceClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetCurrentInstanceId(
    string& instance_id) noexcept {
  instance_id = test_options_->instance_id;
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetCurrentInstanceRegion(
    string& region) noexcept {
  region = test_options_->region;
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetCurrentInstancePublicIpv4Address(
    string& instance_public_ipv4_address) noexcept {
  instance_public_ipv4_address = test_options_->public_ipv4_address;
  return SuccessExecutionResult();
}

ExecutionResult
TestInstanceClientProvider::GetCurrentInstancePrivateIpv4Address(
    string& instance_private_ipv4_address) noexcept {
  instance_private_ipv4_address = test_options_->private_ipv4_address;
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetTagsOfInstance(
    const vector<string>& tag_names, const string& instance_id,
    map<string, string>& tag_values_map) noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetCurrentInstanceProjectId(
    std::string& project_id) noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetCurrentInstanceZone(
    std::string& instance_zone) noexcept {
  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
