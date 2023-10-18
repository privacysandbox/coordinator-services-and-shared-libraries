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

#include "public/cpio/adapters/blob_storage_client/src/blob_storage_client.h"

#include <gtest/gtest.h>

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/blob_storage_client/mock/mock_blob_storage_client_with_overrides.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

using google::cmrt::sdk::blob_storage_service::v1::BlobMetadata;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::cpio::mock::MockBlobStorageClientWithOverrides;
using testing::Return;

namespace google::scp::cpio::test {

class BlobStorageClientTest : public ::testing::Test {
 protected:
  BlobStorageClientTest() { assert(client_.Init().Successful()); }

  MockBlobStorageClientWithOverrides client_;
};

TEST_F(BlobStorageClientTest, GetBlobSuccess) {
  AsyncContext<GetBlobRequest, GetBlobResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlob)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.GetBlob(context), IsSuccessful());
}

TEST_F(BlobStorageClientTest, ListBlobsMetadataSuccess) {
  AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), ListBlobsMetadata)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.ListBlobsMetadata(context), IsSuccessful());
}

TEST_F(BlobStorageClientTest, PutBlobSuccess) {
  AsyncContext<PutBlobRequest, PutBlobResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlob)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.PutBlob(context), IsSuccessful());
}

TEST_F(BlobStorageClientTest, DeleteBlobSuccess) {
  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), DeleteBlob)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.DeleteBlob(context), IsSuccessful());
}

TEST_F(BlobStorageClientTest, GetBlobStreamSuccess) {
  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlobStream)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.GetBlobStream(context), IsSuccessful());
}

TEST_F(BlobStorageClientTest, PutBlobStreamSuccess) {
  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlobStream)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.PutBlobStream(context), IsSuccessful());
}

}  // namespace google::scp::cpio::test
