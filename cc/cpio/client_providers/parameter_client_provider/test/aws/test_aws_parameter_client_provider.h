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

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "cpio/client_providers/parameter_client_provider/src/aws/aws_parameter_client_provider.h"

namespace google::scp::cpio::client_providers {
/// ParameterClientOptions for testing on AWS.
struct TestAwsParameterClientOptions : public ParameterClientOptions {
  std::shared_ptr<std::string> ssm_endpoint_override;
};

/*! @copydoc AwsParameterClientInterface
 */
class TestAwsParameterClientProvider : public AwsParameterClientProvider {
 public:
  TestAwsParameterClientProvider(
      const std::shared_ptr<TestAwsParameterClientOptions>&
          parameter_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider)
      : AwsParameterClientProvider(parameter_client_options,
                                   instance_client_provider),
        ssm_endpoint_override_(
            parameter_client_options &&
                    parameter_client_options->ssm_endpoint_override
                ? parameter_client_options->ssm_endpoint_override
                : std::make_shared<std::string>("")) {}

 protected:
  core::ExecutionResult CreateClientConfiguration(
      std::shared_ptr<Aws::Client::ClientConfiguration>&
          client_parameter) noexcept override;

  std::shared_ptr<std::string> ssm_endpoint_override_;
};
}  // namespace google::scp::cpio::client_providers
