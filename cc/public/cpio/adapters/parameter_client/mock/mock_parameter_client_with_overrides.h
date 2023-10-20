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

#include "cpio/client_providers/parameter_client_provider/mock/mock_parameter_client_provider.h"
#include "public/cpio/adapters/parameter_client/src/parameter_client.h"

namespace google::scp::cpio::mock {
class MockParameterClientWithOverrides : public ParameterClient {
 public:
  MockParameterClientWithOverrides(
      const std::shared_ptr<ParameterClientOptions>& options)
      : ParameterClient(options) {}

  core::ExecutionResult create_parameter_client_provider_result =
      core::SuccessExecutionResult();

  core::ExecutionResult CreateParameterClientProvider() noexcept override {
    if (create_parameter_client_provider_result.Successful()) {
      parameter_client_provider_ = std::make_shared<
          client_providers::mock::MockParameterClientProvider>();
      return create_parameter_client_provider_result;
    }
    return create_parameter_client_provider_result;
  }

  std::shared_ptr<client_providers::mock::MockParameterClientProvider>
  GetParameterClientProvider() {
    return std::dynamic_pointer_cast<
        client_providers::mock::MockParameterClientProvider>(
        parameter_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock
