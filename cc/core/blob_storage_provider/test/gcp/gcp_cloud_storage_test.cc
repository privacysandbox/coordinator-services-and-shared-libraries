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

#include "core/blob_storage_provider/src/gcp/gcp_cloud_storage.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/blob_storage_provider/src/common/error_codes.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "core/utils/src/hashing.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "third_party/cloud_cpp/google/cloud/storage/testing/mock_client.h"

namespace google::scp::core::test {
namespace {

using google::cloud::Status;
using google::cloud::StatusOr;
using CloudStatusCode = google::cloud::StatusCode;
using google::cloud::storage::Client;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::DisableMD5Hash;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::ObjectReadStream;
using google::cloud::storage::Prefix;
using google::cloud::storage::StartOffset;
using google::cloud::storage::internal::EmptyResponse;
using google::cloud::storage::internal::HttpResponse;
using google::cloud::storage::internal::InsertObjectMediaRequest;
using google::cloud::storage::internal::ListObjectsResponse;
using google::cloud::storage::internal::ObjectReadSource;
using google::cloud::storage::internal::ReadSourceResult;
using google::cloud::storage::testing::ClientFromMock;
using google::cloud::storage::testing::MockClient;
using google::cloud::storage::testing::MockObjectReadSource;
using google::cloud::storage::testing::MockStreambuf;
using google::scp::core::DeleteBlobRequest;
using google::scp::core::DeleteBlobResponse;
using google::scp::core::GetBlobRequest;
using google::scp::core::GetBlobResponse;
using google::scp::core::ListBlobsRequest;
using google::scp::core::ListBlobsResponse;
using google::scp::core::LoggerInterface;
using google::scp::core::PutBlobRequest;
using google::scp::core::PutBlobResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::blob_storage_provider::GcpCloudStorageClient;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::utils::Base64Encode;
using google::scp::core::utils::CalculateMd5Hash;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::ByMove;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::FieldsAre;
using testing::InSequence;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Pointee;
using testing::Pointwise;
using testing::Return;

constexpr char kBucketName[] = "bucket";
constexpr char kBlobName1[] = "blob_1";
constexpr char kBlobName2[] = "blob_2";

class GcpCloudStorageClientTest : public testing::Test {
 protected:
  GcpCloudStorageClientTest()
      : mock_client_(make_shared<NiceMock<MockClient>>()),
        gcp_cloud_storage_client_(
            make_shared<Client>(ClientFromMock(mock_client_)),
            make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
            AsyncPriority::Normal, AsyncPriority::Normal) {
    get_blob_context_.request = make_shared<GetBlobRequest>();
    get_blob_context_.callback = [this](auto) { finish_called_ = true; };

    list_blobs_context_.request = make_shared<ListBlobsRequest>();
    list_blobs_context_.callback = [this](auto) { finish_called_ = true; };

    put_blob_context_.request = make_shared<PutBlobRequest>();
    put_blob_context_.callback = [this](auto) { finish_called_ = true; };

    delete_blob_context_.request = make_shared<DeleteBlobRequest>();
    delete_blob_context_.callback = [this](auto) { finish_called_ = true; };
  }

  shared_ptr<MockClient> mock_client_;
  GcpCloudStorageClient gcp_cloud_storage_client_;

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context_;

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context_;

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context_;

  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  std::atomic_bool finish_called_{false};
};

///////////// GetBlob /////////////////////////////////////////////////////////

// Builds an ObjectReadSource that contains the bytes (copied) from input.
StatusOr<unique_ptr<ObjectReadSource>> BuildReadResponseFromBuffer(
    const BytesBuffer& input) {
  // We want the following methods to be called in order, so make an InSequence.
  InSequence seq;
  auto mock_source = make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(true));
  // Copy up to n bytes from input into buf.
  EXPECT_CALL(*mock_source, Read).WillOnce([&input](void* buf, std::size_t n) {
    auto length = std::min(input.length, n);
    std::memcpy(buf, input.bytes->data(), length);
    ReadSourceResult result{length, HttpResponse{200, {}, {}}};

    result.hashes.md5 = *CalculateMd5Hash(input);
    Base64Encode(result.hashes.md5, result.hashes.md5);

    result.size = length;
    return result;
  });
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(false));
  return unique_ptr<ObjectReadSource>(move(mock_source));
}

// Matches arg.bucket_name and arg.object_name with bucket_name and
// blob_name respectively. Also ensures that arg has DisableMD5Hash = false
// and DisableCrc32cChecksum = true.
MATCHER_P2(ReadObjectRequestEqual, bucket_name, blob_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(blob_name), arg.object_name(), result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<DisableMD5Hash>() ||
      arg.template GetOption<DisableMD5Hash>().value()) {
    *result_listener << "Expected ReadObjectRequest to have DisableMD5Hash == "
                        "false and it does not.";
    equal = false;
  }
  if (!arg.template HasOption<DisableCrc32cChecksum>() ||
      !arg.template GetOption<DisableCrc32cChecksum>().value()) {
    *result_listener << "Expected ReadObjectRequest to have "
                        "DisableCrc32cChecksum == true and it does not.";
    equal = false;
  }
  return equal;
}

MATCHER_P(BytesBufferEqual, expected_buffer, "") {
  bool equal = true;
  if (expected_buffer.bytes) {
    if (!arg.bytes) {
      *result_listener << "Actual does not have bytes when we expect it to.";
      equal = false;
    } else {
      string expected_str(expected_buffer.bytes->data(),
                          expected_buffer.length);
      string actual_str(arg.bytes->data(), arg.length);
      equal = ExplainMatchResult(Eq(expected_str), actual_str, result_listener);
    }
  } else if (!ExplainMatchResult(IsNull(), arg.bytes, result_listener)) {
    equal = false;
  }

  if (!ExplainMatchResult(Eq(expected_buffer.length), arg.length,
                          result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpCloudStorageClientTest, GetBlob) {
  get_blob_context_.request->bucket_name = make_shared<string>(kBucketName);
  get_blob_context_.request->blob_name = make_shared<string>(kBlobName1);

  // We add additional capacity to the BytesBuffer to ensure that
  // BytesBuffer::capacity should not be used but BytesBuffer::length should.
  const int extra_length = 10;
  string bytes_str = "response_string";
  BytesBuffer expected_buffer(bytes_str.length() + extra_length);
  expected_buffer.bytes->assign(bytes_str.begin(), bytes_str.end());
  expected_buffer.length = bytes_str.length();

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName1)))
      .WillOnce(Return(ByMove(BuildReadResponseFromBuffer(expected_buffer))));

  get_blob_context_.callback = [this, &expected_buffer](auto& context) {
    EXPECT_SUCCESS(context.result);

    EXPECT_THAT(context.response,
                Pointee(FieldsAre(Pointee(BytesBufferEqual(expected_buffer)))));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.GetBlob(get_blob_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

StatusOr<unique_ptr<ObjectReadSource>> BuildBadHashReadResponse() {
  // We want the following methods to be called in order, so make an InSequence.
  InSequence seq;
  auto mock_source = make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_source, Read).WillOnce([](void* buf, std::size_t n) {
    ReadSourceResult result{0, HttpResponse{200, {}, {}}};
    result.hashes.md5 = "bad";
    return result;
  });
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(false));
  return unique_ptr<ObjectReadSource>(move(mock_source));
}

TEST_F(GcpCloudStorageClientTest, GetBlobHashMismatchFails) {
  get_blob_context_.request->bucket_name = make_shared<string>(kBucketName);
  get_blob_context_.request->blob_name = make_shared<string>(kBlobName1);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName1)))
      .WillOnce(Return(ByMove(BuildBadHashReadResponse())));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::FailureExecutionResult(
                    errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));
    EXPECT_THAT(context.response, IsNull());

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.GetBlob(get_blob_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpCloudStorageClientTest, GetBlobNotFound) {
  get_blob_context_.request->bucket_name = make_shared<string>(kBucketName);
  get_blob_context_.request->blob_name = make_shared<string>(kBlobName1);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName1)))
      .WillOnce(
          Return(ByMove(Status(CloudStatusCode::kNotFound, "Blob not found"))));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::FailureExecutionResult(
                    errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND)));
    EXPECT_THAT(context.response, IsNull());

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.GetBlob(get_blob_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

///////////// ListBlobs ///////////////////////////////////////////////////////

// Matches a ListObjectsRequest with bucket_name and no Prefix.
// Ensures that MaxResults is present and is 1000.
// Ensures StartOffset is not present.
MATCHER_P(ListObjectsRequestEqualNoOffset, bucket_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template GetOption<Prefix>().has_value()) {
    *result_listener
        << "Expected arg to not have a present Prefix value but has: "
        << arg.template GetOption<Prefix>().value();
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(1000),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template HasOption<StartOffset>()) {
    if (auto offset = arg.template GetOption<StartOffset>();
        !offset.value().empty()) {
      *result_listener
          << "Expected ListObjectsRequest to not have StartOffset but has: "
          << offset.value();
      equal = false;
    }
  }
  return equal;
}

MATCHER_P2(BlobEquals, bucket_name, blob_name, "") {
  return ExplainMatchResult(FieldsAre(Pointee(bucket_name), Pointee(blob_name)),
                            arg, result_listener);
}

TEST_F(GcpCloudStorageClientTest, ListBlobsNoPrefix) {
  list_blobs_context_.request->bucket_name = make_shared<string>(kBucketName);

  EXPECT_CALL(*mock_client_,
              ListObjects(ListObjectsRequestEqualNoOffset(kBucketName)))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              },
              {
                "name": "%s"
              }
            ]
          })""",
                          kBlobName1, kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());
    EXPECT_THAT(context.response->blobs,
                Pointee(ElementsAre(BlobEquals(kBucketName, kBlobName1),
                                    BlobEquals(kBucketName, kBlobName2))));
    EXPECT_THAT(context.response->next_marker, IsNull());

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.ListBlobs(list_blobs_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

// Matches a ListObjectsRequest with bucket_name and Prefix(blob_name).
// Ensures that MaxResults is present and is 1000.
// Ensures StartOffset is not present.
MATCHER_P2(ListObjectsRequestEqualNoOffset, bucket_name, blob_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<Prefix>() ||
      !ExplainMatchResult(Eq(blob_name),
                          arg.template GetOption<Prefix>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(1000),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template HasOption<StartOffset>()) {
    if (auto offset = arg.template GetOption<StartOffset>();
        !offset.value().empty()) {
      *result_listener
          << "Expected ListObjectsRequest to not have StartOffset but has: "
          << offset.value();
      equal = false;
    }
  }
  return equal;
}

TEST_F(GcpCloudStorageClientTest, ListBlobsWithPrefix) {
  list_blobs_context_.request->bucket_name = make_shared<string>(kBucketName);
  list_blobs_context_.request->blob_name = make_shared<string>("blob_");

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualNoOffset(
                                 kBucketName, "blob_")))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              },
              {
                "name": "%s"
              }
            ]
          })""",
                          kBlobName1, kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());
    EXPECT_THAT(context.response->blobs,
                Pointee(ElementsAre(BlobEquals(kBucketName, kBlobName1),
                                    BlobEquals(kBucketName, kBlobName2))));
    EXPECT_THAT(context.response->next_marker, IsNull());

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.ListBlobs(list_blobs_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

// Matches a ListObjectsRequest with bucket_name and blob_name.
// Ensures that MaxResults is present and is 1000.
// Ensures StartOffset is present and is offset.
MATCHER_P3(ListObjectsRequestEqualWithOffset, bucket_name, blob_name, offset,
           "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<Prefix>() ||
      !ExplainMatchResult(Eq(blob_name),
                          arg.template GetOption<Prefix>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(1000),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<StartOffset>() ||
      !ExplainMatchResult(Eq(offset),
                          arg.template GetOption<StartOffset>().value(),
                          result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpCloudStorageClientTest, ListBlobsWithMarker) {
  list_blobs_context_.request->bucket_name = make_shared<string>(kBucketName);
  list_blobs_context_.request->blob_name = make_shared<string>("blob_");
  list_blobs_context_.request->marker = make_shared<string>(kBlobName1);

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualWithOffset(
                                 kBucketName, "blob_", kBlobName1)))
      .WillOnce(Return(ByMove(
          ListObjectsResponse::FromHttpResponse(absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              }
            ]
          })""",
                                                                kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());
    EXPECT_THAT(context.response->blobs,
                Pointee(ElementsAre(BlobEquals(kBucketName, kBlobName2))));
    EXPECT_THAT(context.response->next_marker, IsNull());

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.ListBlobs(list_blobs_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpCloudStorageClientTest, ListBlobsWithMarkerSkipsFirstObject) {
  list_blobs_context_.request->bucket_name = make_shared<string>(kBucketName);
  list_blobs_context_.request->blob_name = make_shared<string>("blob_");
  list_blobs_context_.request->marker = make_shared<string>(kBlobName1);

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualWithOffset(
                                 kBucketName, "blob_", kBlobName1)))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              },
              {
                "name": "%s"
              }
            ]
          })""",
                          kBlobName1, kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());
    EXPECT_THAT(context.response->blobs,
                Pointee(ElementsAre(BlobEquals(kBucketName, kBlobName2))));
    EXPECT_THAT(context.response->next_marker, IsNull());

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.ListBlobs(list_blobs_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

// Used for Pointwise matching of Blob -> Blobs using the MATCHER_P version.
MATCHER(BlobsEqual, "") {
  const auto& actual_blob = std::get<0>(arg);
  const auto& expected_blob = std::get<1>(arg);
  return ExplainMatchResult(
      BlobEquals(*expected_blob.bucket_name, *expected_blob.blob_name),
      actual_blob, result_listener);
}

TEST_F(GcpCloudStorageClientTest, ListBlobsReturnsMarkerAndEnforcesPageSize) {
  list_blobs_context_.request->bucket_name = make_shared<string>(kBucketName);
  list_blobs_context_.request->blob_name = make_shared<string>("blob_");

  // Make a JSON object with items named 1 to 1005.
  string items_str;
  for (int64_t i = 1; i <= 1005; i++) {
    if (!items_str.empty()) {
      absl::StrAppend(&items_str, ",");
    }
    absl::StrAppendFormat(&items_str, R"""({"name": "%s"})""",
                          absl::StrCat("blob_", i));
  }

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualNoOffset(
                                 kBucketName, "blob_")))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::StrFormat(R"""({"items": [%s]})""", items_str)))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());

    // We expect to only see blobs 1-1000, not [1001, 1005].
    std::vector<Blob> expected_blobs;
    expected_blobs.reserve(1000);
    for (int64_t i = 1; i <= 1000; i++) {
      expected_blobs.push_back(
          Blob{make_shared<string>(kBucketName),
               make_shared<string>(absl::StrCat("blob_", i))});
    }
    EXPECT_THAT(context.response->blobs,
                Pointee(Pointwise(BlobsEqual(), expected_blobs)));
    EXPECT_THAT(context.response->next_marker,
                Pointee(BlobEquals(kBucketName, "blob_1000")));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.ListBlobs(list_blobs_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpCloudStorageClientTest, ListBlobsPropagatesFailure) {
  list_blobs_context_.request->bucket_name = make_shared<string>(kBucketName);
  list_blobs_context_.request->blob_name = make_shared<string>("blob_");

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualNoOffset(
                                 kBucketName, "blob_")))
      .WillOnce(
          Return(ByMove(Status(CloudStatusCode::kInvalidArgument, "error"))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::FailureExecutionResult(
                    errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.ListBlobs(list_blobs_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

///////////// PutBlob /////////////////////////////////////////////////////////

MATCHER_P(InsertObjectRequestEquals, expected_request, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(expected_request.bucket_name()), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(expected_request.object_name()), arg.object_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(expected_request.contents()), arg.contents(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MD5HashValue>() ||
      !ExplainMatchResult(
          Eq(expected_request.template GetOption<MD5HashValue>().value()),
          arg.template GetOption<MD5HashValue>().value(), result_listener)) {
    *result_listener << "Expected arg has the same MD5 but does not.";
    equal = false;
  }
  return equal;
}

TEST_F(GcpCloudStorageClientTest, PutBlob) {
  put_blob_context_.request->bucket_name = make_shared<string>(kBucketName);
  put_blob_context_.request->blob_name = make_shared<string>(kBlobName1);

  // We add additional capacity to the BytesBuffer to ensure that
  // BytesBuffer::capacity should not be used but BytesBuffer::length should.
  const int extra_length = 10;
  string bytes_str = "put_string";
  put_blob_context_.request->buffer = std::make_shared<BytesBuffer>(bytes_str);
  put_blob_context_.request->buffer->bytes->resize(bytes_str.length() +
                                                   extra_length);
  put_blob_context_.request->buffer->capacity =
      bytes_str.length() + extra_length;

  // Use Google Cloud's MD5 method.
  string expected_md5_hash = google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(kBucketName, kBlobName1, bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_client_,
              InsertObjectMedia(InsertObjectRequestEquals(expected_request)))
      .WillOnce(Return(ObjectMetadata()));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.PutBlob(put_blob_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpCloudStorageClientTest, PutBlobPropagatesFailure) {
  put_blob_context_.request->bucket_name = make_shared<string>(kBucketName);
  put_blob_context_.request->blob_name = make_shared<string>(kBlobName1);

  string bytes_str = "put_string";
  put_blob_context_.request->buffer =
      make_shared<BytesBuffer>(bytes_str.length());
  put_blob_context_.request->buffer->bytes->assign(bytes_str.begin(),
                                                   bytes_str.end());
  put_blob_context_.request->buffer->length = bytes_str.length();

  // Use Google Cloud's MD5 method.
  string expected_md5_hash = google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(kBucketName, kBlobName1, bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_client_,
              InsertObjectMedia(InsertObjectRequestEquals(expected_request)))
      .WillOnce(Return(Status(CloudStatusCode::kInvalidArgument, "failure")));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::FailureExecutionResult(
                    errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.PutBlob(put_blob_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

///////////// DeleteBlob //////////////////////////////////////////////////////

MATCHER_P2(DeleteObjectRequestEquals, bucket_name, blob_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(blob_name), arg.object_name(), result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpCloudStorageClientTest, DeleteBlob) {
  delete_blob_context_.request->bucket_name = make_shared<string>(kBucketName);
  delete_blob_context_.request->blob_name = make_shared<string>(kBlobName1);

  EXPECT_CALL(*mock_client_,
              DeleteObject(DeleteObjectRequestEquals(kBucketName, kBlobName1)))
      .WillOnce(Return(EmptyResponse{}));

  delete_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_.DeleteBlob(delete_blob_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

}  // namespace
}  // namespace google::scp::core::test
