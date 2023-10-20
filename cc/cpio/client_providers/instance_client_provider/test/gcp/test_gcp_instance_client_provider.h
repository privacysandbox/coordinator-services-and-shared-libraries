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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/service_interface.h"
#include "cpio/client_providers/instance_client_provider/test/test_instance_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/test/global_cpio/test_cpio_options.h"

namespace google::scp::cpio::client_providers {
/**
 * @copydoc TestInstanceClientProvider.
 */
class TestGcpInstanceClientProvider : public TestInstanceClientProvider {
 public:
  explicit TestGcpInstanceClientProvider(
      const std::shared_ptr<TestInstanceClientOptions>& test_options)
      : TestInstanceClientProvider(test_options) {}

  core::ExecutionResult GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override;
};
}  // namespace google::scp::cpio::client_providers
