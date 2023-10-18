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

#include <gmock/gmock.h>

#include <memory>

#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"

namespace google::scp::cpio {
class MockBlobStorageClient : public BlobStorageClientInterface {
 public:
  MockBlobStorageClient() {
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
  }

  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetBlob,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, ListBlobsMetadata,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          google::cmrt::sdk::blob_storage_service::v1::
              ListBlobsMetadataResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, PutBlob,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, DeleteBlob,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>)),
      (noexcept, override));

  /// Streaming operations.

  MOCK_METHOD(
      core::ExecutionResult, GetBlobStream,
      ((core::ConsumerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, PutBlobStream,
      ((core::ProducerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>)),
      (noexcept, override));
};

}  // namespace google::scp::cpio
