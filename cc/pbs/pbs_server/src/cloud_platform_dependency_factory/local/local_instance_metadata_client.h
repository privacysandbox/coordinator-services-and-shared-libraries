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

#include <map>
#include <string>
#include <vector>

#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"

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
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::pbs {
/**
 * @copydoc InstanceClientProviderInterface.
 */
class LocalInstanceClientProvider
    : public cpio::client_providers::InstanceClientProviderInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult GetCurrentInstanceResourceName(
      AsyncContext<GetCurrentInstanceResourceNameRequest,
                   GetCurrentInstanceResourceNameResponse>& context) noexcept
      override {
    // Not implemented.
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult GetTagsByResourceName(
      AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
          context) noexcept override {
    // Not implemented.
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult GetInstanceDetailsByResourceName(
      AsyncContext<GetInstanceDetailsByResourceNameRequest,
                   GetInstanceDetailsByResourceNameResponse>& context) noexcept
      override {
    // Not implemented.
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override {
    // this is AWS type of resource name.
    resource_name =
        R"(arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE)";
    return SuccessExecutionResult();
  }

  ExecutionResult GetInstanceDetailsByResourceNameSync(
      const std::string& resource_name,
      InstanceDetails& instance_details) noexcept override {
    // Instance ID need not be supplied for a local instance.
    instance_details.set_instance_id("");
    auto* network = instance_details.add_networks();
    network->set_private_ipv4_address("localhost");
    network->set_public_ipv4_address("127.0.0.1");
    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs
