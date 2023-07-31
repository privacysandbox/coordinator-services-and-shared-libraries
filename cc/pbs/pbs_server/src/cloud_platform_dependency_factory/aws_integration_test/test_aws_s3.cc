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

#include "test_aws_s3.h"

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>

#include "core/blob_storage_provider/src/aws/aws_s3.h"

#include "test_configuration_keys.h"

using Aws::String;
using Aws::Client::ClientConfiguration;
using Aws::S3::S3Client;
using std::make_shared;
using std::string;

namespace google::scp::pbs {
core::ExecutionResult TestAwsS3Provider::CreateClientConfig() noexcept {
  auto execution_result = AwsS3Provider::CreateClientConfig();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  string endpoint_override;
  execution_result =
      config_provider_->Get(kS3EndpointOverride, endpoint_override);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  client_config_->endpointOverride = endpoint_override;

  return core::SuccessExecutionResult();
}

void TestAwsS3Provider::CreateS3() noexcept {
  s3_client_ = make_shared<S3Client>(
      *client_config_,
      Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
      /* use virtual host */ false);
}
}  // namespace google::scp::pbs
