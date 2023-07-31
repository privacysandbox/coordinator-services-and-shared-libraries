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

#include "cpio/client_providers/config_client_provider/src/config_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/test/aws/test_aws_parameter_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/test/config_client/test_aws_config_client_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc ConfigClientProvider
 */
class TestAwsConfigClientProvider : public ConfigClientProvider {
 public:
  explicit TestAwsConfigClientProvider(
      const std::shared_ptr<TestAwsConfigClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider);
};
}  // namespace google::scp::cpio::client_providers
