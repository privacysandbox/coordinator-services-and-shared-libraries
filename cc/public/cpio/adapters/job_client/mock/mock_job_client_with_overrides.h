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

#include "cpio/client_providers/job_client_provider/mock/mock_job_client_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::mock {
class MockJobClientWithOverrides : public JobClient {
 public:
  explicit MockJobClientWithOverrides(
      const std::shared_ptr<JobClientOptions>& options =
          std::make_shared<JobClientOptions>())
      : JobClient(options) {}

  core::ExecutionResult Init() noexcept override {
    job_client_provider_ =
        std::make_shared<client_providers::mock::MockJobClientProvider>();
    return core::SuccessExecutionResult();
  }

  client_providers::mock::MockJobClientProvider& GetJobClientProvider() {
    return static_cast<client_providers::mock::MockJobClientProvider&>(
        *job_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock
