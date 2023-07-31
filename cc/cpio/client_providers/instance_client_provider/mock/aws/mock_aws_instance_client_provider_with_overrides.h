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

#include "cpio/client_providers/instance_client_provider/mock/aws/mock_ec2_client.h"
#include "cpio/client_providers/instance_client_provider/mock/aws/mock_ec2_metadata_client.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"

namespace google::scp::cpio::client_providers::mock {
class MockAwsInstanceClientProviderWithOverrides
    : public AwsInstanceClientProvider {
 public:
  MockAwsInstanceClientProviderWithOverrides() {
    ec2_metadata_client_ = std::make_shared<MockEC2MetadataClient>();
  }

  core::ExecutionResult Run() noexcept override {
    GetEC2MetadataClient()->resource_path_mock =
        "/latest/meta-data/placement/region";
    GetEC2MetadataClient()->resource_mock = "region";
    auto execution_result = AwsInstanceClientProvider::Run();
    if (!execution_result.Successful()) {
      return execution_result;
    }

    ec2_client_ = std::make_shared<MockEC2Client>();
    return core::SuccessExecutionResult();
  }

  std::shared_ptr<MockEC2MetadataClient> GetEC2MetadataClient() {
    return std::dynamic_pointer_cast<MockEC2MetadataClient>(
        ec2_metadata_client_);
  }

  std::shared_ptr<MockEC2Client> GetEC2Client() {
    return std::dynamic_pointer_cast<MockEC2Client>(ec2_client_);
  }
};
}  // namespace google::scp::cpio::client_providers::mock
