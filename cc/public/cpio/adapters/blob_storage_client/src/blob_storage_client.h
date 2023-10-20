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

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "public/cpio/interface/blob_storage_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc BlobStorageClientInterface
 */
class BlobStorageClient : public BlobStorageClientInterface {
 public:
  explicit BlobStorageClient(
      const std::shared_ptr<BlobStorageClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse>
          get_blob_context) noexcept override;

  core::ExecutionResult ListBlobsMetadata(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          google::cmrt::sdk::blob_storage_service::v1::
              ListBlobsMetadataResponse>
          list_blobs_metadata_context) noexcept override;

  core::ExecutionResult PutBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse>
          put_blob_context) noexcept override;

  core::ExecutionResult DeleteBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>
          delete_blob_context) noexcept override;

  /// Streaming operations.

  core::ExecutionResult GetBlobStream(
      core::ConsumerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>
          get_blob_stream_context) noexcept override;

  core::ExecutionResult PutBlobStream(
      core::ProducerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>
          put_blob_stream_context) noexcept override;

 protected:
  std::shared_ptr<client_providers::BlobStorageClientProviderInterface>
      blob_storage_client_provider_;

 private:
  std::shared_ptr<BlobStorageClientOptions> options_;
};
}  // namespace google::scp::cpio
