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
#include <sstream>
#include <string>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/config_provider_interface.h"
#include "google/cloud/storage/client.h"

namespace google::scp::core::blob_storage_provider {
/*! @copydoc BlobStorageClientInterface
 */
class GcpCloudStorageClient : public BlobStorageClientInterface {
 public:
  explicit GcpCloudStorageClient(
      const std::shared_ptr<const google::cloud::storage::Client>
          cloud_storage_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      AsyncPriority async_execution_priority,
      AsyncPriority io_async_execution_priority)
      : cloud_storage_client_shared_(cloud_storage_client),
        async_executor_(async_executor),
        io_async_executor_(io_async_executor),
        async_execution_priority_(async_execution_priority),
        io_async_execution_priority_(io_async_execution_priority) {}

  ExecutionResult GetBlob(AsyncContext<GetBlobRequest, GetBlobResponse>&
                              get_blob_context) noexcept override;

  ExecutionResult ListBlobs(AsyncContext<ListBlobsRequest, ListBlobsResponse>&
                                list_blobs_context) noexcept override;

  ExecutionResult PutBlob(AsyncContext<PutBlobRequest, PutBlobResponse>&
                              put_blob_context) noexcept override;

  ExecutionResult DeleteBlob(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
          delete_blob_context) noexcept override;

 protected:
  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * ReadObject callback.
   *
   * @param get_blob_context The get blob context object.
   */
  virtual void GetBlobAsync(
      AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context) noexcept;

  /**
   * @brief Is called when objects are list and returned from the Cloud Storage
   * ListObjects callback.
   *
   * @param list_blobs_context The list blobs context object.
   */
  virtual void ListBlobAsync(AsyncContext<ListBlobsRequest, ListBlobsResponse>
                                 list_objects_context) noexcept;

  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * InsertObject callback.
   *
   * @param put_blob_context The put blob context object.
   */
  virtual void PutBlobAsync(
      AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context) noexcept;

  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * DeleteObject callback.
   *
   * @param delete_blob_context The delete blob context object.
   */
  virtual void DeleteBlobAsync(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>
          delete_blob_context) noexcept;

 private:
  /// An instance of the GCP GCS client.
  std::shared_ptr<const google::cloud::storage::Client>
      cloud_storage_client_shared_;
  /// An instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  /// An instance of the IO async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
  /// Priorities with which the tasks will be scheduled onto the async
  /// executors.
  const AsyncPriority async_execution_priority_;
  const AsyncPriority io_async_execution_priority_;
};

/*! @copydoc BlobStorageProviderInterface
 */
class GcpCloudStorageProvider : public BlobStorageProviderInterface {
 public:
  explicit GcpCloudStorageProvider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      AsyncPriority async_execution_priority,
      AsyncPriority io_async_execution_priority)
      : async_executor_(async_executor),
        io_async_executor_(io_async_executor),
        config_provider_(config_provider),
        async_execution_priority_(async_execution_priority),
        io_async_execution_priority_(io_async_execution_priority) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult CreateBlobStorageClient(
      std::shared_ptr<BlobStorageClientInterface>& blob_storage_client) noexcept
      override;

 protected:
  /// Creates ClientConfig to create Cloud Storage.
  virtual ExecutionResult CreateClientConfig() noexcept;

  /// Creates Cloud Storage Client.
  virtual void CreateCloudStorage() noexcept;

  /// An instance of the async executor. To execute call
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the IO async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;

  /// An instance of the GCP Cloud Storage client configuration.
  std::shared_ptr<google::cloud::Options> client_config_;

  /// An instance of the GCP Cloud Storage client. For thread safety, copies of
  /// the client are made to avoid multiple threads accessing the same
  /// instance of the client. See the following link:
  /// http://shortn/_Mw9VVdbxhe
  std::shared_ptr<const google::cloud::storage::Client>
      cloud_storage_client_shared_;

  /// An instance of the config provider.
  const std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// Priorities with which the tasks will be scheduled onto the async
  /// executors.
  const AsyncPriority async_execution_priority_;
  const AsyncPriority io_async_execution_priority_;
};
}  // namespace google::scp::core::blob_storage_provider
