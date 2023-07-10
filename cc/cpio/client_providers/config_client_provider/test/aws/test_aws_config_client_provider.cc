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

#include "test_aws_config_client_provider.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/parameter_client_provider/test/aws/test_aws_parameter_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/test/config_client/test_aws_config_client_options.h"

using std::make_shared;
using std::move;
using std::shared_ptr;

namespace google::scp::cpio::client_providers {
TestAwsConfigClientProvider::TestAwsConfigClientProvider(
    const std::shared_ptr<TestAwsConfigClientOptions>& options,
    const std::shared_ptr<InstanceClientProviderInterface>&
        instance_client_provider)
    : ConfigClientProvider(options, instance_client_provider) {
  auto parameter_client_options = make_shared<TestAwsParameterClientOptions>();
  parameter_client_options->ssm_endpoint_override =
      options->ssm_endpoint_override;
  parameter_client_provider_ = ParameterClientProviderFactory::Create(
      move(parameter_client_options), instance_client_provider_);
}

shared_ptr<ConfigClientProviderInterface> ConfigClientProviderFactory::Create(
    const std::shared_ptr<ConfigClientOptions>& options) {
  std::shared_ptr<InstanceClientProviderInterface> instance_client;
  GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(instance_client);
  return make_shared<TestAwsConfigClientProvider>(
      std::dynamic_pointer_cast<TestAwsConfigClientOptions>(options),
      instance_client);
}
}  // namespace google::scp::cpio::client_providers
