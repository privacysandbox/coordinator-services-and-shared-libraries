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
#include <vector>

#include "cpio/client_providers/kms_client_provider/src/aws/tee_aws_kms_client_provider.h"

namespace google::scp::cpio::client_providers::mock {
class MockTeeAwsKmsClientProviderWithOverrides
    : public TeeAwsKmsClientProvider {
 public:
  MockTeeAwsKmsClientProviderWithOverrides(
      const std::shared_ptr<RoleCredentialsProviderInterface>&
          credential_provider)
      : TeeAwsKmsClientProvider(credential_provider) {}

  core::ExecutionResult DecryptUsingEnclavesKmstoolCli(
      const std::string& command, std::string& plaintext) noexcept override {
    plaintext = returned_plaintext;
    return core::SuccessExecutionResult();
  }

  std::string returned_plaintext;
};
}  // namespace google::scp::cpio::client_providers::mock
