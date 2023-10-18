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

#include "cpio/client_providers/kms_client_provider/mock/mock_kms_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/kms_client/src/kms_client.h"

namespace google::scp::cpio::mock {
class MockKmsClientWithOverrides : public KmsClient {
 public:
  explicit MockKmsClientWithOverrides(
      const std::shared_ptr<KmsClientOptions>& options =
          std::make_shared<KmsClientOptions>())
      : KmsClient(options) {}

  core::ExecutionResult Init() noexcept override {
    kms_client_provider_ =
        std::make_shared<client_providers::mock::MockKmsClientProvider>();
    return core::SuccessExecutionResult();
  }

  client_providers::mock::MockKmsClientProvider& GetKmsClientProvider() {
    return static_cast<client_providers::mock::MockKmsClientProvider&>(
        *kms_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock
