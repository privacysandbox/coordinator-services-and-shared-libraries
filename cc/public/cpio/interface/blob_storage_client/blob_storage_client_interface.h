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

#ifndef SCP_CPIO_INTERFACE_BLOB_STORAGE_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_BLOB_STORAGE_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/interface/async_context.h"
#include "core/interface/streaming_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for interacting with BlobStorage.
 *
 * Use BlobStorageClientFactory::Create to create the BlobStorageClient. Call
 * BlobStorageClientInterface::Init and BlobStorageClientInterface::Run before
 * actually usin it, and call BlobStorageClientInterface::Stop when finished
 * using it.
 */
class BlobStorageClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Gets a Blob from storage.
   *
   * @param get_blob_context The context for the operation.
   * @return core::ExecutionResult Scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse>
          get_blob_context) noexcept = 0;

  /**
   * @brief Lists Blobs metadata in storage.
   *
   * @param list_blobs_metadata_context The context for the operation.
   * @return core::ExecutionResult Scheduling result returned synchronously.
   */
  virtual core::ExecutionResult ListBlobsMetadata(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          google::cmrt::sdk::blob_storage_service::v1::
              ListBlobsMetadataResponse>
          list_blobs_metadata_context) noexcept = 0;

  /**
   * @brief Puts (inserts or updates) a Blob in storage.
   *
   * @param put_blob_context The context for the operation.
   * @return core::ExecutionResult Scheduling result returned synchronously.
   */
  virtual core::ExecutionResult PutBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse>
          put_blob_context) noexcept = 0;

  /**
   * @brief Deletes a Blob in storage.
   *
   * @param delete_blob_context The context for the operation.
   * @return core::ExecutionResult Scheduling result returned synchronously.
   */
  virtual core::ExecutionResult DeleteBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>
          delete_blob_context) noexcept = 0;

  /// Streaming operations.

  /**
   * @brief Gets a Blob from storage in a streaming manner.
   *
   * @param get_blob_stream_context The context for the operation.
   * @return core::ExecutionResult Scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetBlobStream(
      core::ConsumerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>
          get_blob_stream_context) noexcept = 0;

  /**
   * @brief Puts a Blob into storage in a streaming manner.
   *
   * @param put_blob_stream_context The context for the operation.
   * @return core::ExecutionResult Scheduling result returned synchronously.
   */
  virtual core::ExecutionResult PutBlobStream(
      core::ProducerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>
          put_blob_stream_context) noexcept = 0;
};

/// Factory to create BlobStorageClient.
class BlobStorageClientFactory {
 public:
  /**
   * @brief Creates BlobStorageClient.
   *
   * @return std::unique_ptr<BlobStorageClient> BlobStorageClient object.
   */
  static std::unique_ptr<BlobStorageClientInterface> Create(
      BlobStorageClientOptions options = BlobStorageClientOptions());
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_BLOB_STORAGE_CLIENT_INTERFACE_H_
