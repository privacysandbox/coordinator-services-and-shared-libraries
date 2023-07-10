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

#include <algorithm>
#include <bitset>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/blob_storage_provider/src/common/error_codes.h"
#include "core/interface/blob_storage_provider_interface.h"

namespace google::scp::core::blob_storage_provider::mock {

inline bool CompareBlobs(Blob& lblob, Blob& rblob) {
  return *lblob.blob_name < *rblob.blob_name;
}

class MockBlobStorageClient : public BlobStorageClientInterface {
 public:
  ExecutionResult GetBlob(AsyncContext<GetBlobRequest, GetBlobResponse>&
                              get_blob_context) noexcept {
    if (get_blob_mock) {
      return get_blob_mock(get_blob_context);
    }
    auto full_path = *get_blob_context.request->bucket_name + std::string("/") +
                     *get_blob_context.request->blob_name;

    std::ifstream input_stream(full_path, std::ios::binary | std::ios::ate);

    if (!input_stream) {
      get_blob_context.result = FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND);
      get_blob_context.Finish();
      return SuccessExecutionResult();
    }

    auto end_offset = input_stream.tellg();
    input_stream.seekg(0, std::ios::beg);

    auto content_length = std::size_t(end_offset - input_stream.tellg());
    get_blob_context.response = std::make_shared<GetBlobResponse>();
    get_blob_context.response->buffer = std::make_shared<BytesBuffer>();
    get_blob_context.response->buffer->length = content_length;

    if (content_length != 0) {
      get_blob_context.response->buffer->bytes =
          std::make_shared<std::vector<Byte>>(content_length);

      if (!input_stream.read(
              reinterpret_cast<char*>(
                  get_blob_context.response->buffer->bytes->data()),
              content_length)) {
        get_blob_context.result = FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB);
        get_blob_context.Finish();
        return SuccessExecutionResult();
      }
    }

    get_blob_context.result = SuccessExecutionResult();
    get_blob_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult ListBlobs(AsyncContext<ListBlobsRequest, ListBlobsResponse>&
                                list_blobs_context) noexcept {
    if (list_blobs_mock) {
      return list_blobs_mock(list_blobs_context);
    }
    auto full_path = *list_blobs_context.request->bucket_name +
                     std::string("/") + *list_blobs_context.request->blob_name;

    list_blobs_context.response = std::make_shared<ListBlobsResponse>();
    list_blobs_context.response->blobs = std::make_shared<std::vector<Blob>>();

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(full_path)) {
      Blob blob;
      blob.blob_name = std::make_shared<std::string>(entry.path());
      list_blobs_context.response->blobs->push_back(std::move(blob));
    }

    std::sort(list_blobs_context.response->blobs->begin(),
              list_blobs_context.response->blobs->end(), CompareBlobs);
    list_blobs_context.result = SuccessExecutionResult();
    list_blobs_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult PutBlob(AsyncContext<PutBlobRequest, PutBlobResponse>&
                              put_blob_context) noexcept {
    if (put_blob_mock) {
      return put_blob_mock(put_blob_context);
    }
    auto full_path = *put_blob_context.request->bucket_name + std::string("/") +
                     *put_blob_context.request->blob_name;

    std::filesystem::path storage_path(full_path);
    std::filesystem::create_directories(storage_path.parent_path());

    std::ofstream output_stream(full_path, std::ofstream::trunc);
    output_stream.write(reinterpret_cast<char*>(
                            put_blob_context.request->buffer->bytes->data()),
                        put_blob_context.request->buffer->length);
    output_stream.close();

    put_blob_context.result = SuccessExecutionResult();
    put_blob_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult DeleteBlob(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
          delete_blob_context) noexcept {
    if (delete_blob_mock) {
      return delete_blob_mock(delete_blob_context);
    }
    auto full_path = *delete_blob_context.request->bucket_name +
                     std::string("/") + *delete_blob_context.request->blob_name;
    if (!std::filesystem::remove_all(full_path)) {
      delete_blob_context.result = FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND);
      delete_blob_context.Finish();
      return SuccessExecutionResult();
    }

    delete_blob_context.result = SuccessExecutionResult();
    delete_blob_context.Finish();
    return SuccessExecutionResult();
  }

  std::function<ExecutionResult(AsyncContext<GetBlobRequest, GetBlobResponse>&)>
      get_blob_mock;
  std::function<ExecutionResult(
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&)>
      list_blobs_mock;
  std::function<ExecutionResult(AsyncContext<PutBlobRequest, PutBlobResponse>&)>
      put_blob_mock;
  std::function<ExecutionResult(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&)>
      delete_blob_mock;
};

class MockBlobStorageProvider : public BlobStorageProviderInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult CreateBlobStorageClient(
      std::shared_ptr<BlobStorageClientInterface>& blob_storage_client) noexcept
      override {
    blob_storage_client = std::make_shared<MockBlobStorageClient>();
    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::core::blob_storage_provider::mock
