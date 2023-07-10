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
#include <vector>

#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/mock/aws/mock_ssm_client.h"
#include "cpio/client_providers/parameter_client_provider/src/aws/aws_parameter_client_provider.h"

namespace google::scp::cpio::client_providers::mock {
class MockAwsParameterClientProviderWithOverrides
    : public AwsParameterClientProvider {
 public:
  MockAwsParameterClientProviderWithOverrides()
      : AwsParameterClientProvider(
            std::make_shared<ParameterClientOptions>(),
            std::make_shared<MockInstanceClientProvider>()) {}

  std::shared_ptr<MockInstanceClientProvider> GetInstanceClientProvider() {
    return std::dynamic_pointer_cast<MockInstanceClientProvider>(
        instance_client_provider_);
  }

  std::shared_ptr<MockSSMClient> GetSSMClient() {
    return std::dynamic_pointer_cast<MockSSMClient>(ssm_client_);
  }

  core::ExecutionResult Init() noexcept override {
    auto execution_result = AwsParameterClientProvider::Init();
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    ssm_client_ = std::make_shared<MockSSMClient>();
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::cpio::client_providers::mock
