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

#include <functional>
#include <memory>

#include "cpio/client_providers/global_cpio/src/cpio_provider/lib_cpio_provider.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockLibCpioProviderWithOverrides : public LibCpioProvider {
 public:
  MockLibCpioProviderWithOverrides()
      : LibCpioProvider(std::make_shared<CpioOptions>()) {}

  core::ExecutionResult GetInstanceClientProvider(
      std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider) noexcept override {
    instance_client_provider_ = std::make_shared<MockInstanceClientProvider>();
    instance_client_provider = instance_client_provider_;
    return core::SuccessExecutionResult();
  }

  std::shared_ptr<core::AsyncExecutorInterface> GetCpuAsyncExecutorMember() {
    return cpu_async_executor_;
  }

  std::shared_ptr<core::AsyncExecutorInterface> GetIoAsyncExecutorMember() {
    return io_async_executor_;
  }

  std::shared_ptr<core::HttpClientInterface> GetHttp2ClientMember() {
    return http2_client_;
  }

  std::shared_ptr<core::HttpClientInterface> GetHttp1ClientMember() {
    return http1_client_;
  }

  std::shared_ptr<InstanceClientProviderInterface>
  GetInstanceClientProviderMember() {
    return instance_client_provider_;
  }

  std::shared_ptr<RoleCredentialsProviderInterface>
  GetRoleCredentialsProviderMember() {
    return role_credentials_provider_;
  }

  std::shared_ptr<AuthTokenProviderInterface> GetAuthTokenProviderMember() {
    return auth_token_provider_;
  }
};
}  // namespace google::scp::cpio::client_providers::mock
