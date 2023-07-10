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

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>

#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/config_provider_interface.h"

namespace google::scp::core::blob_storage_provider {
/*! @copydoc BlobStorageClientInterface
 */
class AwsS3Client : public BlobStorageClientInterface {
 public:
  explicit AwsS3Client(
      const std::shared_ptr<Aws::S3::S3Client>& s3_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor)
      : s3_client_(s3_client), async_executor_(async_executor) {}

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
   * @brief Is called when the object is returned from the S3 GetObject
   * callback.
   *
   * @param get_blob_context The get blob context object.
   * @param s3_client An instance of the S3 client.
   * @param get_object_request The get object request.
   * @param get_object_outcome The get object outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  virtual void OnGetObjectCallback(
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::GetObjectRequest& get_object_request,
      const Aws::S3::Model::GetObjectOutcome& get_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when objects are list and returned from the S3 ListObjects
   * callback.
   *
   * @param list_blobs_context The list blobs context object.
   * @param s3_client An instance of the S3 client.
   * @param list_object_request The list objects request.
   * @param list_object_outcome The list objects outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  virtual void OnListObjectsCallback(
      AsyncContext<ListBlobsRequest, ListBlobsResponse>& list_objects_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::ListObjectsRequest& list_objects_request,
      const Aws::S3::Model::ListObjectsOutcome& list_objects_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the object is returned from the S3 PutObject
   * callback.
   *
   * @param put_blob_context The put blob context object.
   * @param s3_client An instance of the S3 client.
   * @param put_object_request The put object request.
   * @param put_object_outcome The put object outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  virtual void OnPutObjectCallback(
      AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::PutObjectRequest& put_object_request,
      const Aws::S3::Model::PutObjectOutcome& put_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the object is returned from the S3 DeleteObject
   * callback.
   *
   * @param delete_blob_context The delete blob context object.
   * @param s3_client An instance of the S3 client.
   * @param delete_object_request The delete object request.
   * @param delete_object_outcome The delete object outcome of the async
   * operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  virtual void OnDeleteObjectCallback(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& delete_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::DeleteObjectRequest& delete_object_request,
      const Aws::S3::Model::DeleteObjectOutcome& delete_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

 private:
  /// An instance of the AWS S3 client.
  std::shared_ptr<Aws::S3::S3Client> s3_client_;
  /// An instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
};

/*! @copydoc BlobStorageProviderInterface
 */
class AwsS3Provider : public BlobStorageProviderInterface {
 public:
  explicit AwsS3Provider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : async_executor_(async_executor),
        io_async_executor_(io_async_executor),
        config_provider_(config_provider) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult CreateBlobStorageClient(
      std::shared_ptr<BlobStorageClientInterface>& blob_storage_client) noexcept
      override;

 protected:
  /// Creates ClientConfig to create S3Client.
  virtual ExecutionResult CreateClientConfig() noexcept;
  /// Creates S3Client.
  virtual void CreateS3() noexcept;

  /// An instance of the async executor. To execute call
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the IO async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;

  /// An instance of the AWS client configuration.
  std::shared_ptr<Aws::Client::ClientConfiguration> client_config_;

  /// An instance of the AWS S3 client.
  std::shared_ptr<Aws::S3::S3Client> s3_client_;

  /// An instance of the config provider.
  const std::shared_ptr<core::ConfigProviderInterface> config_provider_;
};
}  // namespace google::scp::core::blob_storage_provider
