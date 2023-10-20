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

#include "cpio/client_providers/interface/blob_storage_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {

/*! @copydoc BlobStorageClientProviderInterface
 */
class MockBlobStorageClientProvider
    : public BlobStorageClientProviderInterface {
 public:
  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetBlob,
              ((core::AsyncContext<
                  cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                  cmrt::sdk::blob_storage_service::v1::GetBlobResponse>&)),
              (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetBlobStream,
      ((core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, ListBlobsMetadata,
      ((core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>&)),
      (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, PutBlob,
              ((core::AsyncContext<
                  cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                  cmrt::sdk::blob_storage_service::v1::PutBlobResponse>&)),
              (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, PutBlobStream,
      ((core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&)),
      (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, DeleteBlob,
              ((core::AsyncContext<
                  cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
                  cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>&)),
              (noexcept, override));
};

}  // namespace google::scp::cpio::client_providers::mock
