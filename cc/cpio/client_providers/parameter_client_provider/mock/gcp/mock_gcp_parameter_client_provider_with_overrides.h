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
#include <utility>
#include <vector>

#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/src/gcp/gcp_parameter_client_provider.h"

namespace google::scp::cpio::client_providers::mock {

class MockGcpParameterClientProviderOverrides
    : public GcpParameterClientProvider {
 public:
  explicit MockGcpParameterClientProviderOverrides(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider)
      : GcpParameterClientProvider(async_executor, io_async_executor,
                                   instance_client_provider,
                                   std::make_shared<ParameterClientOptions>()) {
  }

  std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
      secret_manager_mock;

  std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
  GetSecretManagerClient() noexcept override {
    if (secret_manager_mock) {
      return secret_manager_mock;
    }
    return nullptr;
  }
};
}  // namespace google::scp::cpio::client_providers::mock
