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

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/http2_client/mock/mock_http_client.h"
#include "cpio/client_providers/global_cpio/src/cpio_provider/lib_cpio_provider.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"

namespace google::scp::cpio::client_providers::mock {
class MockLibCpioProvider : public LibCpioProvider {
 public:
  MockLibCpioProvider() : LibCpioProvider(std::make_shared<CpioOptions>()) {
    instance_client_provider_ = std::make_shared<MockInstanceClientProvider>();
    cpu_async_executor_ =
        std::make_shared<core::async_executor::mock::MockAsyncExecutor>();
    http2_client_ =
        std::make_shared<core::http2_client::mock::MockHttpClient>();
    role_credentials_provider_ =
        std::make_shared<mock::MockRoleCredentialsProvider>();
  }
};
}  // namespace google::scp::cpio::client_providers::mock
