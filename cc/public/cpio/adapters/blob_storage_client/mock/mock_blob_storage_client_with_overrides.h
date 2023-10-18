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

#include "cpio/client_providers/blob_storage_client_provider/mock/mock_blob_storage_client_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::mock {
class MockBlobStorageClientWithOverrides : public BlobStorageClient {
 public:
  explicit MockBlobStorageClientWithOverrides(
      const std::shared_ptr<BlobStorageClientOptions>& options =
          std::make_shared<BlobStorageClientOptions>())
      : BlobStorageClient(options) {}

  core::ExecutionResult Init() noexcept override {
    blob_storage_client_provider_ = std::make_shared<
        client_providers::mock::MockBlobStorageClientProvider>();
    return core::SuccessExecutionResult();
  }

  client_providers::mock::MockBlobStorageClientProvider&
  GetBlobStorageClientProvider() {
    return static_cast<client_providers::mock::MockBlobStorageClientProvider&>(
        *blob_storage_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock
