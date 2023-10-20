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

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/global_cpio/src/cpio_provider/lib_cpio_provider.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/cpio/test/global_cpio/test_cpio_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc LibCpioProvider
 */
class TestLibCpioProvider : public LibCpioProvider {
 public:
  explicit TestLibCpioProvider(
      const std::shared_ptr<TestCpioOptions>& test_options);

 private:
  std::shared_ptr<RoleCredentialsProviderInterface>
  CreateRoleCredentialsProvider(
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept override;

  std::shared_ptr<TestCpioOptions> test_options_;
};
}  // namespace google::scp::cpio::client_providers
