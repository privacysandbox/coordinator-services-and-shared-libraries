// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::distance;
using std::ifstream;
using std::ios;
using std::make_shared;
using std::ofstream;
using std::shared_ptr;
using std::string;
using std::vector;
using std::filesystem::create_directory;
using std::filesystem::current_path;
using std::filesystem::directory_iterator;

namespace google::scp::core::test {
TEST(MockBlobStorageProviderTest, GetBlob) {
  MockBlobStorageProvider mock_blob_storage_provider;
  current_path("/tmp");

  atomic<bool> condition = false;
  create_directory("bucket_get");

  string file_content = "1234";
  vector<char> bytes(file_content.begin(), file_content.end());

  ofstream output_stream("bucket_get/1.txt", ios::trunc | ios::binary);
  output_stream << file_content;
  output_stream.close();

  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      make_shared<GetBlobRequest>(),
      [&](AsyncContext<GetBlobRequest, GetBlobResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->buffer->length, 4);
        EXPECT_EQ(context.response->buffer->bytes->size(), bytes.size());

        for (size_t i = 0; i < bytes.size(); ++i) {
          EXPECT_EQ(context.response->buffer->bytes->at(i), bytes[i]);
        }
        condition = true;
      });

  get_blob_context.request->bucket_name = make_shared<string>("bucket_get");
  get_blob_context.request->blob_name = make_shared<string>("1.txt");
  EXPECT_SUCCESS(blob_storage_client->GetBlob(get_blob_context));
}

TEST(MockBlobStorageProviderTest, PutBlob) {
  MockBlobStorageProvider mock_blob_storage_provider;
  current_path("/tmp");

  atomic<bool> condition = false;
  create_directory("bucket_put");

  string file_content = "1234";
  vector<Byte> bytes(file_content.begin(), file_content.end());

  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context(
      make_shared<PutBlobRequest>(),
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& context) {
        EXPECT_SUCCESS(context.result);

        ifstream input_stream("bucket_put/test_hash/1.txt",
                              ios::ate | ios::binary);
        auto content_length = input_stream.tellg();
        input_stream.seekg(0);
        EXPECT_EQ(bytes.size(), content_length);

        vector<char> to_read(content_length);
        input_stream.read(to_read.data(), content_length);

        for (size_t i = 0; i < to_read.size(); ++i) {
          EXPECT_EQ(bytes[i], to_read[i]);
        }
        condition = true;
      });

  put_blob_context.request->bucket_name = make_shared<string>("bucket_put");
  put_blob_context.request->blob_name = make_shared<string>("test_hash/1.txt");
  put_blob_context.request->buffer = make_shared<BytesBuffer>();
  put_blob_context.request->buffer->bytes = make_shared<vector<Byte>>(bytes);
  put_blob_context.request->buffer->length = bytes.size();
  EXPECT_SUCCESS(blob_storage_client->PutBlob(put_blob_context));
}

TEST(MockBlobStorageProviderTest, DeleteBlob) {
  MockBlobStorageProvider mock_blob_storage_provider;
  current_path("/tmp");

  atomic<bool> condition = false;
  create_directory("bucket_delete");

  string file_content = "1234";
  vector<char> bytes(file_content.begin(), file_content.end());

  ofstream output_stream("bucket_delete/2.txt", ios::trunc | ios::binary);
  output_stream << file_content;
  output_stream.close();

  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context(
      make_shared<DeleteBlobRequest>(),
      [&](AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& context) {
        EXPECT_SUCCESS(context.result);

        EXPECT_EQ(
            distance(directory_iterator("bucket_delete"), directory_iterator{}),
            0);
        condition = true;
      });

  delete_blob_context.request->bucket_name =
      make_shared<string>("bucket_delete");
  delete_blob_context.request->blob_name = make_shared<string>("2.txt");
  EXPECT_SUCCESS(blob_storage_client->DeleteBlob(delete_blob_context));
}

TEST(MockBlobStorageProviderTest, ListBlobs) {
  MockBlobStorageProvider mock_blob_storage_provider;
  current_path("/tmp");

  atomic<bool> condition = false;
  create_directory("bucket_list");
  create_directory("bucket_list/1");
  create_directory("bucket_list/1/3");
  create_directory("bucket_list/2");

  ofstream output_stream1("bucket_list/2.txt", ios::trunc | ios::binary);
  output_stream1.close();

  ofstream output_stream2("bucket_list/1/2.txt", ios::trunc | ios::binary);
  output_stream2.close();

  ofstream output_stream3("bucket_list/1/3/4.txt", ios::trunc | ios::binary);
  output_stream3.close();

  ofstream output_stream4("bucket_list/2/5.txt", ios::trunc | ios::binary);
  output_stream4.close();

  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context(
      make_shared<ListBlobsRequest>(),
      [&](AsyncContext<ListBlobsRequest, ListBlobsResponse>& context) {
        EXPECT_SUCCESS(context.result);

        EXPECT_EQ(context.response->blobs->size(), 7);
        EXPECT_EQ(*context.response->blobs->at(0).blob_name, "1");
        EXPECT_EQ(*context.response->blobs->at(1).blob_name, "1/2.txt");
        EXPECT_EQ(*context.response->blobs->at(2).blob_name, "1/3");
        EXPECT_EQ(*context.response->blobs->at(3).blob_name, "1/3/4.txt");
        EXPECT_EQ(*context.response->blobs->at(4).blob_name, "2");
        EXPECT_EQ(*context.response->blobs->at(6).blob_name, "2/5.txt");
        EXPECT_EQ(*context.response->blobs->at(5).blob_name, "2.txt");

        condition = true;
      });

  list_blobs_context.request->bucket_name = make_shared<string>("bucket_list");
  list_blobs_context.request->blob_name = make_shared<string>("");
  EXPECT_SUCCESS(blob_storage_client->ListBlobs(list_blobs_context));
}

}  // namespace google::scp::core::test
