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

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/service_interface.h"
#include "core/interface/streaming_context.h"
#include "core/interface/type_def.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/cpio/interface/blob_storage_client/type_def.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

namespace google::scp::cpio::client_providers {

/// BlobStorageClientProviderInterface provide cloud blob storage access
/// functionalities.
class BlobStorageClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~BlobStorageClientProviderInterface() = default;

  /**
   * @brief Used to download a blob using blob identifiers.
   *
   * @param get_blob_context The get blob context object to download the blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult GetBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::GetBlobResponse>&
          get_blob_context) noexcept = 0;

  /**
   * @brief Used to download a blob using blob identifiers.
   *
   * @param get_blob_context The get blob context object to download the blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult GetBlobStream(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
          get_blob_stream_context) noexcept = 0;

  /**
   * @brief Used to list metadata of blobs using blob identifiers.
   *
   * @param list_blobs_context The list blobs context object to list all the
   * blobs in a range.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult ListBlobsMetadata(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>&
          list_blobs_context) noexcept = 0;

  /**
   * @brief Used to create a blob using blob identifiers.
   *
   * @param put_blob_context The put blob context object to create a blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult PutBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::PutBlobResponse>&
          put_blob_context) noexcept = 0;

  /**
   * @brief Used to create a blob using blob identifiers.
   *
   * @param put_blob_context The put blob context object to create a blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult PutBlobStream(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context) noexcept = 0;

  /**
   * @brief Used to delete a blob using blob identifiers.
   *
   * @param delete_blob_context The delete blob context object to create a
   * blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult DeleteBlob(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>&
          delete_blob_context) noexcept = 0;
};

/**
 * @brief BlobStorageClientProviderFactory provides cloud blob storage
 * functionalities by allowing the caller to create a blob storage client.
 */
class BlobStorageClientProviderFactory {
 public:
  static std::shared_ptr<BlobStorageClientProviderInterface> Create(
      std::shared_ptr<BlobStorageClientOptions> options,
      std::shared_ptr<InstanceClientProviderInterface> instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept;
};

}  // namespace google::scp::cpio::client_providers
