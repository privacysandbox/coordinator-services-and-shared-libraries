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
#include "core/interface/config_provider_interface.h"
#include "core/interface/streaming_context.h"
#include "cpio/client_providers/blob_storage_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "google/cloud/storage/client.h"
#include "public/cpio/interface/blob_storage_client/type_def.h"

namespace google::scp::cpio::client_providers {

class GcpCloudStorageFactory;

/*! @copydoc BlobStorageClientProviderInterface
 */
class GcpCloudStorageClientProvider
    : public BlobStorageClientProviderInterface {
 public:
  explicit GcpCloudStorageClientProvider(
      std::shared_ptr<BlobStorageClientOptions> options,
      std::shared_ptr<InstanceClientProviderInterface> instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      std::shared_ptr<GcpCloudStorageFactory> cloud_storage_factory =
          std::make_shared<GcpCloudStorageFactory>())
      : options_(options),
        instance_client_(instance_client),
        cloud_storage_factory_(cloud_storage_factory),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::GetBlobResponse>&
          get_blob_context) noexcept override;

  core::ExecutionResult GetBlobStream(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
          get_blob_stream_context) noexcept override;

  core::ExecutionResult ListBlobsMetadata(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>&
          list_blobs_context) noexcept override;

  core::ExecutionResult PutBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::PutBlobResponse>&
          put_blob_context) noexcept override;

  core::ExecutionResult PutBlobStream(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context) noexcept override;

  core::ExecutionResult DeleteBlob(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>&
          delete_blob_context) noexcept override;

 private:
  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * ReadObject callback.
   *
   * @param get_blob_context The get blob context object.
   */
  void GetBlobInternal(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::GetBlobResponse>
          get_blob_context) noexcept;

  // Housekeeping object for tracking the progress of a single GetBlobStream.
  struct GetBlobStreamTracker {
    // The stream to read bytes out of.
    google::cloud::storage::ObjectReadStream stream;
    // The ending index of the previous read. -1 indicates we are starting at
    // the true 0 index of the Blob.
    uint64_t last_end_byte_index = -1;
    // How many bytes remain to be read out of stream.
    size_t bytes_remaining = 0;
  };

  // Starts a GetBlobStream read and returns the associated tracker.
  core::ExecutionResultOr<std::shared_ptr<GetBlobStreamTracker>>
  InitGetBlobStreamTracker(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
          context) noexcept;

  // Reads the next portion out of tracker.stream and returns a
  // GetBlobStreamResponse from it. Updates trackers members.
  cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse ReadNextPortion(
      const cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest& request,
      GetBlobStreamTracker& tracker) noexcept;

  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * ReadObject callback.
   *
   * @param get_blob_stream_context The get blob stream context object.
   */
  void GetBlobStreamInternal(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>
          get_blob_stream_context,
      std::shared_ptr<GetBlobStreamTracker> tracker) noexcept;

  /**
   * @brief Is called when objects are list and returned from the Cloud Storage
   * ListObjects callback.
   *
   * @param list_blobs_context The list blobs context object.
   */
  void ListBlobsMetadataInternal(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>
          list_objects_context) noexcept;

  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * InsertObject callback.
   *
   * @param put_blob_context The put blob context object.
   */
  void PutBlobInternal(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::PutBlobResponse>
          put_blob_context) noexcept;

  struct PutBlobStreamTracker {
    // The stream to write contents to.
    google::cloud::storage::ObjectWriteStream stream;
    // If present, stream is invalid and should be resumed using this ID.
    // Otherwise, we can write into stream.
    std::optional<std::string> session_id;
    // The expected bucket and blob name for this upload. If this is different
    // at any point in the upload, the upload fails.
    std::string bucket_name, blob_name;

    // Timestamp in nanoseconds of when this PutBlobStream session should
    // expire.
    std::chrono::nanoseconds expiry_time_ns =
        std::chrono::duration<int64_t>::min();
  };

  void InitPutBlobStream(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>
          put_blob_stream_context) noexcept;

  void RestoreUploadIfSuspended(
      PutBlobStreamTracker& tracker,
      google::cloud::storage::Client& cloud_storage_client) noexcept;

  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * InsertObject callback.
   *
   * @param put_blob_stream_context The put blob context object.
   * @param tracker The tracker for this specific upload.
   */
  void PutBlobStreamInternal(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>
          put_blob_stream_context,
      std::shared_ptr<PutBlobStreamTracker> tracker) noexcept;

  /**
   * @brief Is called when the object is returned from the Cloud Storage
   * DeleteObject callback.
   *
   * @param delete_blob_context The delete blob context object.
   */
  void DeleteBlobInternal(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>
          delete_blob_context) noexcept;

  // Checks if stream has an error and finishes context if it does. This is
  // unlikely to happen.
  template <typename Context, typename Stream>
  core::ExecutionResult ValidateStream(Context& context,
                                       const Stream& stream) noexcept {
    constexpr bool is_read =
        std::is_same_v<Stream, google::cloud::storage::ObjectReadStream>;
    constexpr bool is_write =
        std::is_same_v<Stream, google::cloud::storage::ObjectWriteStream>;
    static_assert(is_read || is_write);
    // This is called for GetBlob, GetBlobStream, and PutBlobStream. In the
    // former case, we should not call FinishStreamingContext.
    constexpr bool is_get_blob = std::is_same_v<
        core::AsyncContext<
            cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
            cmrt::sdk::blob_storage_service::v1::GetBlobResponse>,
        Context>;

    google::cloud::Status status;
    if constexpr (is_read) {
      status = stream.status();
    } else if constexpr (is_write) {
      status = stream.last_status();
    }
    auto result = core::SuccessExecutionResult();
    if (!status.ok()) {
      result = common::GcpUtils::GcpErrorConverter(status);
      SCP_ERROR_CONTEXT(kGcpCloudStorageClientProvider, context, result,
                        "Blob stream failed. Message: %s.",
                        status.message().c_str());
      if constexpr (is_get_blob) {
        FinishContext(result, context, cpu_async_executor_);
      } else {
        FinishStreamingContext(result, context, cpu_async_executor_);
      }
    }
    return result;
  }

  std::shared_ptr<BlobStorageClientOptions> options_;

  std::shared_ptr<InstanceClientProviderInterface> instance_client_;

  // An instance of the factory for cloud::storage::Client.
  std::shared_ptr<GcpCloudStorageFactory> cloud_storage_factory_;

  /// An instance of the GCP GCS client.
  std::shared_ptr<const google::cloud::storage::Client>
      cloud_storage_client_shared_;
  /// An instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  static constexpr char kGcpCloudStorageClientProvider[] =
      "GcpCloudStorageClientProvider";
};

/// Creates GCP cloud::storage::Client
class GcpCloudStorageFactory {
 public:
  virtual core::ExecutionResultOr<
      std::shared_ptr<google::cloud::storage::Client>>
  CreateClient(std::shared_ptr<BlobStorageClientOptions> options,
               const std::string& project_id) noexcept;

  virtual ~GcpCloudStorageFactory() = default;
};

}  // namespace google::scp::cpio::client_providers
