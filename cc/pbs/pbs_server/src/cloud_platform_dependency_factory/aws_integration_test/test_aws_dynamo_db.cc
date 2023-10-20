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

#include "test_aws_dynamo_db.h"

#include <string>

#include <aws/core/Aws.h>

#include "core/common/uuid/src/uuid.h"
#include "core/nosql_database_provider/src/aws/aws_dynamo_db.h"

#include "test_configuration_keys.h"

using Aws::Client::ClientConfiguration;
using std::string;

using google::scp::core::common::kZeroUuid;

namespace google::scp::pbs {
core::ExecutionResult TestAwsDynamoDB::CreateClientConfig() noexcept {
  auto execution_result = AwsDynamoDB::CreateClientConfig();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  string endpoint_override;
  execution_result =
      config_provider_->Get(kDynamoDbEndpointOverride, endpoint_override);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  client_config_->endpointOverride = endpoint_override;

  return core::SuccessExecutionResult();
}
}  // namespace google::scp::pbs
