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

#include <google/protobuf/util/message_differencer.h>

#include "cc/cpio/client_providers/parameter_client_provider/mock/mock_parameter_client_provider.h"
#include "cpio/client_providers/config_client_provider/src/config_client_provider.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "google/protobuf/any.pb.h"

namespace google::scp::cpio::client_providers::mock {
class MockConfigClientProviderWithOverrides : public ConfigClientProvider {
 public:
  explicit MockConfigClientProviderWithOverrides(
      const std::shared_ptr<ConfigClientOptions>& config_client_options)
      : ConfigClientProvider(config_client_options,
                             std::make_shared<MockInstanceClientProvider>()) {
    parameter_client_provider_ =
        std::make_shared<MockParameterClientProvider>();
  }

  std::shared_ptr<MockInstanceClientProvider> GetInstanceClientProvider() {
    return std::dynamic_pointer_cast<MockInstanceClientProvider>(
        instance_client_provider_);
  }

  std::shared_ptr<MockParameterClientProvider> GetParameterClientProvider() {
    return std::dynamic_pointer_cast<MockParameterClientProvider>(
        parameter_client_provider_);
  }
};
}  // namespace google::scp::cpio::client_providers::mock
