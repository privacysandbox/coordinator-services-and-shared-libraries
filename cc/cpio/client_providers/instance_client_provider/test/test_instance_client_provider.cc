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

#include "absl/strings/str_format.h"
#include "public/core/interface/execution_result.h"

using absl::StrFormat;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {
constexpr char kAwsResourceNameFormat[] = R"(arn:aws:ec2:%s:%s:instance/%s)";
}  // namespace

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

ExecutionResult
TestInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    const std::string& resource_name,
    cmrt::sdk::instance_service::v1::InstanceDetails&
        instance_details) noexcept {
  instance_details.set_instance_id(test_options_->instance_id);
  auto* network = instance_details.add_networks();
  network->set_private_ipv4_address(test_options_->private_ipv4_address);
  network->set_public_ipv4_address(test_options_->public_ipv4_address);
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>& context) noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        context) noexcept {
  // Not implemented.
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>& context) noexcept {
  // Not implemented.
  return SuccessExecutionResult();
}

ExecutionResult TestInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  resource_name =
      StrFormat(kAwsResourceNameFormat, test_options_->region,
                test_options_->owner_id, test_options_->instance_id);
  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
