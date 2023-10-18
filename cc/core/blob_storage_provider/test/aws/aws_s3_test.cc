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

#include "core/blob_storage_provider/src/aws/aws_s3.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/PutObjectRequest.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/blob_storage_provider/mock/aws/mock_aws_s3_client.h"
#include "core/blob_storage_provider/mock/aws/mock_s3_client.h"
#include "core/blob_storage_provider/src/common/error_codes.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::IOStream;
using Aws::MakeShared;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::StringStream;
using Aws::Vector;
using Aws::Client::AsyncCallerContext;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using Aws::S3::DeleteObjectResponseReceivedHandler;
using Aws::S3::GetObjectResponseReceivedHandler;
using Aws::S3::ListObjectsResponseReceivedHandler;
using Aws::S3::PutObjectResponseReceivedHandler;
using Aws::S3::S3Client;
using Aws::S3::S3Errors;
using Aws::S3::Model::DeleteObjectOutcome;
using Aws::S3::Model::DeleteObjectRequest;
using Aws::S3::Model::DeleteObjectResult;
using Aws::S3::Model::GetObjectOutcome;
using Aws::S3::Model::GetObjectRequest;
using Aws::S3::Model::GetObjectResult;
using Aws::S3::Model::ListObjectsOutcome;
using Aws::S3::Model::ListObjectsRequest;
using Aws::S3::Model::ListObjectsResult;
using Aws::S3::Model::PutObjectOutcome;
using Aws::S3::Model::PutObjectRequest;
using Aws::S3::Model::PutObjectResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::blob_storage_provider::AwsS3Client;
using google::scp::core::blob_storage_provider::aws::mock::MockS3Client;
using google::scp::core::blob_storage_provider::mock::MockAwsS3Client;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::core::test {
// Tests for AwsS3.
class AwsS3Tests : public ::testing::Test {
 protected:
  void SetUp() override { InitAPI(options_); }

  void TearDown() override { ShutdownAPI(options_); }

  SDKOptions options_;
};

TEST_F(AwsS3Tests, GetBlob) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  mock_s3_client->get_object_async_mock =
      [](const GetObjectRequest& get_object_request,
         const GetObjectResponseReceivedHandler&,
         const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(get_object_request.GetBucket(), "bucket_name");
        EXPECT_EQ(get_object_request.GetKey(), "blob_name");
      };

  MockAwsS3Client aws_s3_client(s3_client, async_executor);
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.request = make_shared<GetBlobRequest>();
  get_blob_context.request->blob_name = make_shared<string>("blob_name");
  get_blob_context.request->bucket_name = make_shared<string>("bucket_name");

  EXPECT_SUCCESS(aws_s3_client.GetBlob(get_blob_context));
}

TEST_F(AwsS3Tests, OnGetObjectCallbackWithError) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  MockAwsS3Client aws_s3_client(s3_client, async_executor);

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.request = make_shared<GetBlobRequest>();
  get_blob_context.request->blob_name = make_shared<string>("blob_name");
  get_blob_context.request->bucket_name = make_shared<string>("bucket_name");
  get_blob_context.callback =
      [](AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
        EXPECT_THAT(get_blob_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));
      };

  GetObjectRequest get_object_request;
  AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
  GetObjectOutcome get_object_outcome(s3_error);

  aws_s3_client.OnGetObjectCallback(get_blob_context, NULL, get_object_request,
                                    get_object_outcome, nullptr);
}

TEST_F(AwsS3Tests, OnGetObjectCallback) {
  GetObjectRequest get_object_request;
  GetObjectResult get_object_result;
  auto input_data = new StringStream("");
  *input_data << "Hello world!";

  // AWS S3 SDK calls free right after object destruction. The ownership of the
  // stream belongs to the test so by adding blocks we are tricking the SDK to
  // think it has the ownership.
  {
    auto async_executor = make_shared<MockAsyncExecutor>();
    auto mock_s3_client = make_shared<MockS3Client>();
    auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

    MockAwsS3Client aws_s3_client(s3_client, async_executor);

    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
    get_blob_context.request = make_shared<GetBlobRequest>();
    get_blob_context.request->blob_name = make_shared<string>("blob_name");
    get_blob_context.request->bucket_name = make_shared<string>("bucket_name");
    get_blob_context.callback =
        [](AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
          EXPECT_SUCCESS(get_blob_context.result);
          EXPECT_EQ(get_blob_context.response->buffer->length, 12);
          EXPECT_EQ(get_blob_context.response->buffer->capacity, 12);
          EXPECT_EQ(get_blob_context.response->buffer->bytes->size(), 12);
          string str("Hello world!");
          for (int i = 0; i < 12; ++i) {
            EXPECT_EQ(get_blob_context.response->buffer->bytes->at(i), str[i]);
          }
        };

    get_object_result.ReplaceBody(input_data);
    get_object_result.SetContentLength(12);
    GetObjectOutcome get_object_outcome(move(get_object_result));
    aws_s3_client.OnGetObjectCallback(get_blob_context, NULL,
                                      get_object_request, get_object_outcome,
                                      nullptr);
  }
}

TEST_F(AwsS3Tests, ListBlobs) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  mock_s3_client->list_objects_async_mock =
      [](const ListObjectsRequest& list_objects_request,
         const ListObjectsResponseReceivedHandler&,
         const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(list_objects_request.GetBucket(), "bucket_name");
        EXPECT_EQ(list_objects_request.GetPrefix(), "");
        EXPECT_EQ(list_objects_request.GetMarker(), "");
      };

  MockAwsS3Client aws_s3_client(s3_client, async_executor);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.request = make_shared<ListBlobsRequest>();
  list_blobs_context.request->bucket_name = make_shared<string>("bucket_name");

  EXPECT_SUCCESS(aws_s3_client.ListBlobs(list_blobs_context));
}

TEST_F(AwsS3Tests, ListBlobsWithPrefix) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  mock_s3_client->list_objects_async_mock =
      [](const ListObjectsRequest& list_objects_request,
         const ListObjectsResponseReceivedHandler&,
         const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(list_objects_request.GetBucket(), "bucket_name");
        EXPECT_EQ(list_objects_request.GetPrefix(), "blob_name");
        EXPECT_EQ(list_objects_request.GetMarker(), "");
      };

  MockAwsS3Client aws_s3_client(s3_client, async_executor);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.request = make_shared<ListBlobsRequest>();
  list_blobs_context.request->blob_name = make_shared<string>("blob_name");
  list_blobs_context.request->bucket_name = make_shared<string>("bucket_name");

  EXPECT_SUCCESS(aws_s3_client.ListBlobs(list_blobs_context));
}

TEST_F(AwsS3Tests, ListBlobsMarker) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  mock_s3_client->list_objects_async_mock =
      [](const ListObjectsRequest& list_objects_request,
         const ListObjectsResponseReceivedHandler&,
         const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(list_objects_request.GetBucket(), "bucket_name");
        EXPECT_EQ(list_objects_request.GetPrefix(), "blob_name");
        EXPECT_EQ(list_objects_request.GetMarker(), "marker");
      };

  MockAwsS3Client aws_s3_client(s3_client, async_executor);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.request = make_shared<ListBlobsRequest>();
  list_blobs_context.request->blob_name = make_shared<string>("blob_name");
  list_blobs_context.request->bucket_name = make_shared<string>("bucket_name");
  list_blobs_context.request->marker = make_shared<string>("marker");

  EXPECT_SUCCESS(aws_s3_client.ListBlobs(list_blobs_context));
}

TEST_F(AwsS3Tests, OnListObjectsCallbackWithError) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  MockAwsS3Client aws_s3_client(s3_client, async_executor);

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.request = make_shared<ListBlobsRequest>();
  list_blobs_context.request->blob_name = make_shared<string>("blob_name");
  list_blobs_context.request->bucket_name = make_shared<string>("bucket_name");
  list_blobs_context.callback =
      [](AsyncContext<ListBlobsRequest, ListBlobsResponse>&
             list_blobs_context) {
        EXPECT_THAT(list_blobs_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));
      };

  ListObjectsRequest list_objects_request;
  AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
  ListObjectsOutcome list_objects_outcome(s3_error);

  aws_s3_client.OnListObjectsCallback(list_blobs_context, NULL,
                                      list_objects_request,
                                      list_objects_outcome, nullptr);
}

TEST_F(AwsS3Tests, OnListObjectsCallback) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  MockAwsS3Client aws_s3_client(s3_client, async_executor);

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.request = make_shared<ListBlobsRequest>();
  list_blobs_context.request->blob_name = make_shared<string>("blob_name");
  list_blobs_context.request->bucket_name = make_shared<string>("bucket_name");
  list_blobs_context.callback =
      [](AsyncContext<ListBlobsRequest, ListBlobsResponse>&
             list_blobs_context) { EXPECT_SUCCESS(list_blobs_context.result); };

  ListObjectsRequest list_objects_request;
  ListObjectsResult list_objects_result;

  // TODO: S3-CRT is needed to mock the objects in the result.
  ListObjectsOutcome list_objects_outcome(list_objects_result);

  aws_s3_client.OnListObjectsCallback(list_blobs_context, NULL,
                                      list_objects_request,
                                      list_objects_outcome, nullptr);
}

TEST_F(AwsS3Tests, PutBlob) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  vector<Byte> bytes = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
  mock_s3_client->put_object_async_mock =
      [&](const PutObjectRequest& put_object_request,
          const PutObjectResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(put_object_request.GetBucket(), "bucket_name");
        EXPECT_EQ(put_object_request.GetKey(), "blob_name");
        char buffer[10];
        put_object_request.GetBody()->read(buffer, 10);
        for (int i = 0; i < 10; i++) {
          EXPECT_EQ(buffer[i], bytes[i]);
        }
      };

  MockAwsS3Client aws_s3_client(s3_client, async_executor);
  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
  put_blob_context.request = make_shared<PutBlobRequest>();
  put_blob_context.request->blob_name = make_shared<string>("blob_name");
  put_blob_context.request->bucket_name = make_shared<string>("bucket_name");
  put_blob_context.request->buffer = make_shared<BytesBuffer>();

  put_blob_context.request->buffer->bytes = make_shared<vector<Byte>>(bytes);
  put_blob_context.request->buffer->length = 10;
  put_blob_context.request->buffer->capacity = 10;

  EXPECT_SUCCESS(aws_s3_client.PutBlob(put_blob_context));
}

TEST_F(AwsS3Tests, OnPutObjectCallbackWithError) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  MockAwsS3Client aws_s3_client(s3_client, async_executor);

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
  put_blob_context.request = make_shared<PutBlobRequest>();
  put_blob_context.request->blob_name = make_shared<string>("blob_name");
  put_blob_context.request->bucket_name = make_shared<string>("bucket_name");
  put_blob_context.callback =
      [](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        EXPECT_THAT(put_blob_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));
      };

  PutObjectRequest put_object_request;
  AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
  PutObjectOutcome put_object_outcome(s3_error);

  aws_s3_client.OnPutObjectCallback(put_blob_context, NULL, put_object_request,
                                    put_object_outcome, nullptr);
}

TEST_F(AwsS3Tests, OnPutObjectCallback) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  MockAwsS3Client aws_s3_client(s3_client, async_executor);

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
  put_blob_context.request = make_shared<PutBlobRequest>();
  put_blob_context.request->blob_name = make_shared<string>("blob_name");
  put_blob_context.request->bucket_name = make_shared<string>("bucket_name");
  put_blob_context.callback =
      [](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        EXPECT_SUCCESS(put_blob_context.result);
      };

  PutObjectRequest put_object_request;
  PutObjectResult put_object_result;
  PutObjectOutcome put_object_outcome(move(put_object_result));
  aws_s3_client.OnPutObjectCallback(put_blob_context, NULL, put_object_request,
                                    put_object_outcome, nullptr);
}

TEST_F(AwsS3Tests, DeleteBlob) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  mock_s3_client->delete_object_async_mock =
      [&](const DeleteObjectRequest& delete_object_request,
          const DeleteObjectResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(delete_object_request.GetBucket(), "bucket_name");
        EXPECT_EQ(delete_object_request.GetKey(), "blob_name");
      };

  MockAwsS3Client aws_s3_client(s3_client, async_executor);
  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context;
  delete_blob_context.request = make_shared<DeleteBlobRequest>();
  delete_blob_context.request->blob_name = make_shared<string>("blob_name");
  delete_blob_context.request->bucket_name = make_shared<string>("bucket_name");

  EXPECT_SUCCESS(aws_s3_client.DeleteBlob(delete_blob_context));
}

TEST_F(AwsS3Tests, OnDeleteObjectCallbackWithError) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  MockAwsS3Client aws_s3_client(s3_client, async_executor);

  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context;
  delete_blob_context.request = make_shared<DeleteBlobRequest>();
  delete_blob_context.request->blob_name = make_shared<string>("blob_name");
  delete_blob_context.request->bucket_name = make_shared<string>("bucket_name");
  delete_blob_context.callback =
      [](AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
             delete_blob_context) {
        EXPECT_THAT(delete_blob_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));
      };

  DeleteObjectRequest delete_object_request;
  AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
  DeleteObjectOutcome delete_object_outcome(s3_error);

  aws_s3_client.OnDeleteObjectCallback(delete_blob_context, NULL,
                                       delete_object_request,
                                       delete_object_outcome, nullptr);
}

TEST_F(AwsS3Tests, OnDeleteObjectCallback) {
  auto async_executor = make_shared<MockAsyncExecutor>();
  auto mock_s3_client = make_shared<MockS3Client>();
  auto s3_client = dynamic_pointer_cast<S3Client>(mock_s3_client);

  MockAwsS3Client aws_s3_client(s3_client, async_executor);

  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context;
  delete_blob_context.request = make_shared<DeleteBlobRequest>();
  delete_blob_context.request->blob_name = make_shared<string>("blob_name");
  delete_blob_context.request->bucket_name = make_shared<string>("bucket_name");
  delete_blob_context.callback =
      [](AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
             delete_blob_context) {
        EXPECT_SUCCESS(delete_blob_context.result);
      };

  DeleteObjectRequest delete_object_request;
  DeleteObjectResult delete_object_result;
  DeleteObjectOutcome delete_object_outcome(move(delete_object_result));
  aws_s3_client.OnDeleteObjectCallback(delete_blob_context, NULL,
                                       delete_object_request,
                                       delete_object_outcome, nullptr);
}
}  // namespace google::scp::core::test
