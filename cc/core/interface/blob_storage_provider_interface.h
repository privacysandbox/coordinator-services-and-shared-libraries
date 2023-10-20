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

#include "async_context.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {

/// Represents a blob associated metdata.
struct Blob {
  /// The bucket name.
  std::shared_ptr<std::string> bucket_name;
  /// The blob name.
  std::shared_ptr<std::string> blob_name;
};

/// Represents a blob request object.
struct BlobRequest {
  /// The bucket name.
  std::shared_ptr<std::string> bucket_name;
  /// The blob name. Note: Overloaded use as prefix for ListBlobs.
  std::shared_ptr<std::string> blob_name;
};

/// Represents the get blob request object.
struct GetBlobRequest : BlobRequest {};

/// Represents the get blob reponse object.
struct GetBlobResponse {
  /// Buffer to keep blob data.
  std::shared_ptr<BytesBuffer> buffer;
};

/// Represents the list blobs request object.
struct ListBlobsRequest : BlobRequest {
  /**
   * @brief Is used to continue a list operation. In a case of numerous results,
   * the service will return a marker to be used on the next call as a
   * pagination token.
   * The list will only include keys that occur lexicographically after the
   * marker.
   */
  std::shared_ptr<std::string> marker;
};

/// Represents the list blobs response object.
struct ListBlobsResponse {
  /// List of blobs returned for the path.
  std::shared_ptr<std::vector<Blob>> blobs;
  /**
   * @brief The marker is required, if blobs name end up on two different
   * partitions on the server side. If marker exist, the caller must continue
   * until marker is null.
   */
  std::shared_ptr<Blob> next_marker;
};

/// Represents the put blob request object.
struct PutBlobRequest : BlobRequest {
  /// Buffer to be written to the blob.
  std::shared_ptr<BytesBuffer> buffer;
};

/// Represents the put blob response object.
struct PutBlobResponse {};

/// Represents the delete blob request object.
struct DeleteBlobRequest : BlobRequest {};

/// Represents the delete blob response object.
struct DeleteBlobResponse {};

/// BlobStorageClient provide cloud blob storage access functionalities.
class BlobStorageClientInterface {
 public:
  virtual ~BlobStorageClientInterface() = default;

  /**
   * @brief Used to download a blob using blob identifiers.
   *
   * @param get_blob_context The get blob context object to download the blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult GetBlob(AsyncContext<GetBlobRequest, GetBlobResponse>&
                                      get_blob_context) noexcept = 0;

  /**
   * @brief Used to list blobs using blob identifiers.
   *
   * @param list_blobs_context The list blobs context object to list all the
   * blobs in a range.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ListBlobs(
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&
          list_blobs_context) noexcept = 0;

  /**
   * @brief Used to create a blob using blob identifiers.
   *
   * @param put_blob_context The put blob context object to create a blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult PutBlob(AsyncContext<PutBlobRequest, PutBlobResponse>&
                                      put_blob_context) noexcept = 0;

  /**
   * @brief Used to delete a blob using blob identifiers.
   *
   * @param delete_blob_context The delete blob context object to create a blob.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult DeleteBlob(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
          delete_blob_context) noexcept = 0;
};

/**
 * @brief BlobStorageProvider provides cloud blob storage functionalities by
 * allowing the caller to create a blob storage client.
 */
class BlobStorageProviderInterface : public ServiceInterface {
 public:
  virtual ~BlobStorageProviderInterface() = default;

  virtual ExecutionResult CreateBlobStorageClient(
      std::shared_ptr<BlobStorageClientInterface>&
          blob_storage_client) noexcept = 0;
};
}  // namespace google::scp::core
