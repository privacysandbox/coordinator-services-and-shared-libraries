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
#include <string>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "core/utils/src/hashing.h"
#include "cpio/client_providers/blob_storage_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/blob_storage_client_provider/src/gcp/gcp_cloud_storage_client_provider.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "third_party/cloud_cpp/google/cloud/storage/testing/mock_client.h"

using google::cloud::Status;
using google::cloud::StatusOr;
using CloudStatusCode = google::cloud::StatusCode;
using google::cloud::storage::Client;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::DisableMD5Hash;
using google::cloud::storage::LimitedErrorCountRetryPolicy;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::Prefix;
using google::cloud::storage::ReadRange;
using google::cloud::storage::internal::ConstBuffer;
using google::cloud::storage::internal::ConstBufferSequence;
using google::cloud::storage::internal::CreateResumableUploadResponse;
using google::cloud::storage::internal::HttpResponse;
using google::cloud::storage::internal::ObjectReadSource;
using google::cloud::storage::internal::QueryResumableUploadResponse;
using google::cloud::storage::internal::ReadSourceResult;
using google::cloud::storage::internal::ResumableUploadRequest;
using google::cloud::storage::internal::UploadChunkRequest;
using google::cloud::storage::testing::ClientFromMock;
using google::cloud::storage::testing::MockClient;
using google::cloud::storage::testing::MockObjectReadSource;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncExecutor;
using google::scp::core::BytesBuffer;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::RetryExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::ConcurrentQueue;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::errors::SC_STREAMING_CONTEXT_DONE;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::core::utils::CalculateMd5Hash;
using google::scp::cpio::client_providers::GcpCloudStorageClientProvider;
using google::scp::cpio::client_providers::GcpCloudStorageFactory;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using testing::ByMove;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::NiceMock;
using testing::Pointwise;
using testing::Return;

namespace google::scp::cpio::client_providers::test {
namespace {
constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr char kBucketName[] = "bucket";
constexpr char kBlobName[] = "blob";

constexpr int64_t kStreamKeepAliveMicrosCount = 100;

class MockGcpCloudStorageFactory : public GcpCloudStorageFactory {
 public:
  MOCK_METHOD(core::ExecutionResultOr<shared_ptr<Client>>, CreateClient,
              (shared_ptr<BlobStorageClientOptions>, const string&),
              (noexcept, override));
};

class GcpCloudStorageClientProviderStreamTest : public testing::Test {
 protected:
  GcpCloudStorageClientProviderStreamTest()
      : instance_client_(make_shared<MockInstanceClientProvider>()),
        storage_factory_(make_shared<NiceMock<MockGcpCloudStorageFactory>>()),
        mock_client_(make_shared<NiceMock<MockClient>>()),
        gcp_cloud_storage_client_(
            make_shared<BlobStorageClientOptions>(), instance_client_,
            make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
            storage_factory_) {
    ON_CALL(*storage_factory_, CreateClient)
        .WillByDefault(
            Return(make_shared<Client>(ClientFromMock(mock_client_))));
    instance_client_->instance_resource_name = kInstanceResourceName;
    get_blob_stream_context_.request = make_shared<GetBlobStreamRequest>();
    get_blob_stream_context_.process_callback = [this](auto, bool) {
      finish_called_ = true;
    };

    put_blob_stream_context_.request = make_shared<PutBlobStreamRequest>();
    put_blob_stream_context_.callback = [this](auto) { finish_called_ = true; };

    EXPECT_SUCCESS(gcp_cloud_storage_client_.Init());
    EXPECT_SUCCESS(gcp_cloud_storage_client_.Run());
  }

  ~GcpCloudStorageClientProviderStreamTest() {
    EXPECT_SUCCESS(gcp_cloud_storage_client_.Stop());
  }

  shared_ptr<MockInstanceClientProvider> instance_client_;
  shared_ptr<MockGcpCloudStorageFactory> storage_factory_;
  shared_ptr<MockClient> mock_client_;
  GcpCloudStorageClientProvider gcp_cloud_storage_client_;

  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
      get_blob_stream_context_;

  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
      put_blob_stream_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  std::atomic_bool finish_called_{false};
};

///////////// GetBlobStream ///////////////////////////////////////////////////

// Compares 2 BlobMetadata's bucket_name and blob_name.
MATCHER_P(BlobMetadataEquals, expected_metadata, "") {
  return ExplainMatchResult(arg.bucket_name(), expected_metadata.bucket_name(),
                            result_listener) &&
         ExplainMatchResult(arg.blob_name(), expected_metadata.blob_name(),
                            result_listener);
}

// Compares 2 Blobs, their metadata and data.
MATCHER_P(BlobEquals, expected_blob, "") {
  return ExplainMatchResult(BlobMetadataEquals(expected_blob.metadata()),
                            arg.metadata(), result_listener) &&
         ExplainMatchResult(expected_blob.data(), arg.data(), result_listener);
}

MATCHER_P(GetBlobStreamResponseEquals, expected, "") {
  return ExplainMatchResult(BlobEquals(expected.blob_portion()),
                            arg.blob_portion(), result_listener) &&
         ExplainMatchResult(expected.byte_range().begin_byte_index(),
                            arg.byte_range().begin_byte_index(),
                            result_listener) &&
         ExplainMatchResult(expected.byte_range().end_byte_index(),
                            arg.byte_range().end_byte_index(), result_listener);
}

MATCHER(GetBlobStreamResponseEquals, "") {
  const auto& [actual, expected] = arg;
  return ExplainMatchResult(GetBlobStreamResponseEquals(expected), actual,
                            result_listener);
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

// Builds an ObjectReadSource that contains the bytes (copied) from bytes_str.
StatusOr<unique_ptr<ObjectReadSource>> BuildReadResponseFromString(
    const string& bytes_str) {
  // We want the following methods to be called in order, so make an InSequence.
  InSequence seq;
  auto mock_source = make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(true));
  // Copy up to n bytes from input into buf.
  EXPECT_CALL(*mock_source, Read)
      .WillOnce([bytes_str = bytes_str](void* buf, std::size_t n) {
        BytesBuffer buffer(bytes_str.length());
        buffer.bytes->assign(bytes_str.begin(), bytes_str.end());
        buffer.length = bytes_str.length();
        auto length = std::min(buffer.length, n);
        std::memcpy(buf, buffer.bytes->data(), length);
        ReadSourceResult result{length, HttpResponse{200, {}, {}}};

        result.hashes.md5 = *CalculateMd5Hash(buffer);
        Base64Encode(result.hashes.md5, result.hashes.md5);

        result.size = length;
        return result;
      });
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(false));
  return unique_ptr<ObjectReadSource>(move(mock_source));
}

TEST_F(GcpCloudStorageClientProviderStreamTest, GetBlobStream) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);

  // 15 chars.
  string bytes_str = "response_string";
  GetBlobStreamResponse expected_response;
  expected_response.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      get_blob_stream_context_.request->blob_metadata());
  *expected_response.mutable_blob_portion()->mutable_data() = bytes_str;
  expected_response.mutable_byte_range()->set_begin_byte_index(0);
  expected_response.mutable_byte_range()->set_end_byte_index(14);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  vector<GetBlobStreamResponse> actual_responses;
  get_blob_stream_context_.process_callback = [this, &actual_responses](
                                                  auto& context, bool) {
    auto resp = context.TryGetNextResponse();
    if (resp != nullptr) {
      actual_responses.push_back(move(*resp));
    } else {
      if (!context.IsMarkedDone()) {
        ADD_FAILURE();
      }
      EXPECT_SUCCESS(context.result);
      finish_called_ = true;
    }
  };

  EXPECT_THAT(gcp_cloud_storage_client_.GetBlobStream(get_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              ElementsAre(GetBlobStreamResponseEquals(expected_response)));
}

TEST_F(GcpCloudStorageClientProviderStreamTest,
       GetBlobStreamMultipleResponses) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.request->set_max_bytes_per_response(2);

  // 15 chars.
  string bytes_str = "response_string";
  // Expect to get responses with data: ["re", "sp", ... "g"]
  vector<GetBlobStreamResponse> expected_responses;
  for (int i = 0; i < bytes_str.length(); i += 2) {
    GetBlobStreamResponse resp;
    resp.mutable_blob_portion()->mutable_metadata()->CopyFrom(
        get_blob_stream_context_.request->blob_metadata());
    // The last 1 character is by itself
    if (i + 1 == bytes_str.length()) {
      *resp.mutable_blob_portion()->mutable_data() = bytes_str.substr(i, 1);
      resp.mutable_byte_range()->set_begin_byte_index(i);
      resp.mutable_byte_range()->set_end_byte_index(i);
    } else {
      *resp.mutable_blob_portion()->mutable_data() = bytes_str.substr(i, 2);
      resp.mutable_byte_range()->set_begin_byte_index(i);
      resp.mutable_byte_range()->set_end_byte_index(i + 1);
    }
    expected_responses.push_back(resp);
  }

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  vector<GetBlobStreamResponse> actual_responses;
  get_blob_stream_context_.process_callback = [this, &actual_responses](
                                                  auto& context, bool) {
    auto resp = context.TryGetNextResponse();
    if (resp != nullptr) {
      actual_responses.push_back(move(*resp));
    } else {
      if (!context.IsMarkedDone()) {
        ADD_FAILURE();
      }
      EXPECT_SUCCESS(context.result);
      finish_called_ = true;
    }
  };

  EXPECT_THAT(gcp_cloud_storage_client_.GetBlobStream(get_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              Pointwise(GetBlobStreamResponseEquals(), expected_responses));
}

TEST_F(GcpCloudStorageClientProviderStreamTest, GetBlobStreamByteRange) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.request->set_max_bytes_per_response(3);
  get_blob_stream_context_.request->mutable_byte_range()->set_begin_byte_index(
      3);
  get_blob_stream_context_.request->mutable_byte_range()->set_end_byte_index(6);

  // We slice "response_string" to indices 3-6. We pad "a" at the end so
  // "content_length" is still 15 to simulate a ranged read.
  string bytes_str = "ponsaaaaaaaaaaa";
  // Expect to get responses with data: ["pon", "s"]
  vector<GetBlobStreamResponse> expected_responses;
  GetBlobStreamResponse resp1, resp2;
  resp1.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      get_blob_stream_context_.request->blob_metadata());
  resp2.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      get_blob_stream_context_.request->blob_metadata());
  *resp1.mutable_blob_portion()->mutable_data() = "pon";
  resp1.mutable_byte_range()->set_begin_byte_index(3);
  resp1.mutable_byte_range()->set_end_byte_index(5);
  *resp2.mutable_blob_portion()->mutable_data() = "s";
  resp2.mutable_byte_range()->set_begin_byte_index(6);
  resp2.mutable_byte_range()->set_end_byte_index(6);
  expected_responses.push_back(resp1);
  expected_responses.push_back(resp2);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  vector<GetBlobStreamResponse> actual_responses;
  get_blob_stream_context_.process_callback = [this, &actual_responses](
                                                  auto& context, bool) {
    auto resp = context.TryGetNextResponse();
    if (resp != nullptr) {
      actual_responses.push_back(move(*resp));
    } else {
      if (!context.IsMarkedDone()) {
        ADD_FAILURE();
      }
      EXPECT_SUCCESS(context.result);
      finish_called_ = true;
    }
  };

  EXPECT_THAT(gcp_cloud_storage_client_.GetBlobStream(get_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              Pointwise(GetBlobStreamResponseEquals(), expected_responses));
}

TEST_F(GcpCloudStorageClientProviderStreamTest, GetBlobStreamFailsIfQueueDone) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.MarkDone();

  // 15 chars.
  string bytes_str = "response_string";

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  get_blob_stream_context_.process_callback = [this](auto& context, bool) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_STREAMING_CONTEXT_DONE)));
    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.GetBlobStream(get_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpCloudStorageClientProviderStreamTest,
       GetBlobStreamFailsIfRequestCancelled) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.TryCancel();

  // 15 chars.
  string bytes_str = "response_string";

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  get_blob_stream_context_.process_callback = [this](auto& context, bool) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED)));
    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.GetBlobStream(get_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
}

///////////// PutBlobStream ///////////////////////////////////////////////////

MATCHER_P(CreateResumableUploadEquals, expected, "") {
  return ExplainMatchResult(expected.bucket_name(), arg.bucket_name(),
                            result_listener) &&
         ExplainMatchResult(expected.object_name(), arg.object_name(),
                            result_listener);
}

MATCHER_P(UploadChunkEquals, expected, "") {
  return ExplainMatchResult(expected.upload_session_url(),
                            arg.upload_session_url(), result_listener) &&
         ExplainMatchResult(expected.offset(), arg.offset(), result_listener) &&
         ExplainMatchResult(ElementsAreArray(expected.payload()), arg.payload(),
                            result_listener);
}

// Note, string s is not copied or moved - it is referred to wherever this
// buffer exists. Therefore, s must continue to be valid while this buffer
// exists.
ConstBufferSequence MakeBuffer(const string& s) {
  return ConstBufferSequence{ConstBuffer(s.c_str(), s.length())};
}

ConstBufferSequence EmptyBuffer() {
  return ConstBufferSequence();
}

MATCHER_P(HasSessionUrl, url, "") {
  return ExplainMatchResult(url, arg.upload_session_url(), result_listener);
}

/**
 * @brief Expects calls to mock_client resembling a resumable upload process.
 * Generally this is the process:
 * 1. CreateResumableUpload
 * 2. (optional) RestoreResumableUpload
 * 3. UploadChunk
 * 4. Loop back to 2
 *
 * @param mock_client Client to set expectations on.
 * @param bucket The name of the bucket these operations occur on.
 * @param blob The name of the blob these operations occur on.
 * @param initial_part The initial data contained in the first request.
 * @param other_parts List of other chunks of data to expect.
 * @param expect_queries Whether or not to expect RestoreResumableUpload's
 * before *each* UploadChunk call.
 */
void ExpectResumableUpload(MockClient& mock_client, const string& bucket,
                           const string& blob, const string& initial_part,
                           const vector<string>& other_parts,
                           bool expect_queries = false) {
  static int upload_count = 0;
  auto session_id = absl::StrFormat("session_%d", upload_count++);
  InSequence seq;

  // First, create a session and upload the initial part.
  uint64_t next_offset = initial_part.length();
  EXPECT_CALL(mock_client, CreateResumableUpload(CreateResumableUploadEquals(
                               ResumableUploadRequest(bucket, blob))))
      .WillOnce(Return(CreateResumableUploadResponse{session_id}));

  EXPECT_CALL(mock_client, UploadChunk(UploadChunkEquals(UploadChunkRequest(
                               session_id, 0, MakeBuffer(initial_part)))))
      .WillOnce(
          Return(QueryResumableUploadResponse{next_offset, std::nullopt}));

  // For each of the other parts, we expect to get another UploadChunk call.
  for (auto it = other_parts.begin(); it != other_parts.end(); it++) {
    if (expect_queries) {
      EXPECT_CALL(mock_client, QueryResumableUpload(HasSessionUrl(session_id)))
          .WillRepeatedly(
              Return(QueryResumableUploadResponse{next_offset, std::nullopt}));
    }
    EXPECT_CALL(mock_client, UploadChunk(UploadChunkEquals(UploadChunkRequest(
                                 session_id, next_offset, MakeBuffer(*it)))))
        .WillOnce(Return(QueryResumableUploadResponse{
            next_offset + it->length(), std::nullopt}));
    next_offset += it->length();
  }
  // Finalization call - no body but should return ObjectMetadata.
  EXPECT_CALL(mock_client,
              UploadChunk(UploadChunkEquals(UploadChunkRequest(
                  session_id, next_offset, EmptyBuffer(), {} /*hash_values*/))))
      .WillOnce(
          Return(QueryResumableUploadResponse{next_offset, ObjectMetadata{}}));
}

TEST_F(GcpCloudStorageClientProviderStreamTest, PutBlobStream) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str = "initial";
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // No additional request objects.
  put_blob_stream_context_.MarkDone();

  ExpectResumableUpload(*mock_client_, kBucketName, kBlobName, bytes_str, {});

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpCloudStorageClientProviderStreamTest, PutBlobStreamMultiplePortions) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  // The API will optimize uploads to MockClient::kUploadSize bytes, we test our
  // implementation by making each part that size.
  string initial_str(MockClient::kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(
      initial_str);

  vector<string> strings{string(MockClient::kUploadSize, 'b'),
                         string(MockClient::kUploadSize, 'c')};
  auto request2 = *put_blob_stream_context_.request;
  request2.mutable_blob_portion()->set_data(strings[0]);
  auto request3 = *put_blob_stream_context_.request;
  request3.mutable_blob_portion()->set_data(strings[1]);
  put_blob_stream_context_.TryPushRequest(move(request2));
  put_blob_stream_context_.TryPushRequest(move(request3));
  put_blob_stream_context_.MarkDone();

  ExpectResumableUpload(*mock_client_, kBucketName, kBlobName, initial_str,
                        strings);

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpCloudStorageClientProviderStreamTest,
       PutBlobStreamMultiplePortionsWithNoOpCycles) {
  // In order to test the "no message" path, we must have real async executors.
  auto cpu_async_executor = make_shared<AsyncExecutor>(2, 10),
       io_async_executor = make_shared<AsyncExecutor>(2, 10);
  EXPECT_SUCCESS(cpu_async_executor->Init());
  EXPECT_SUCCESS(io_async_executor->Init());
  EXPECT_SUCCESS(cpu_async_executor->Run());
  EXPECT_SUCCESS(io_async_executor->Run());
  GcpCloudStorageClientProvider async_client(
      make_shared<BlobStorageClientOptions>(), instance_client_,
      cpu_async_executor, io_async_executor, storage_factory_);
  EXPECT_SUCCESS(async_client.Init());
  EXPECT_SUCCESS(async_client.Run());
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  // The API will optimize uploads to MockClient::kUploadSize bytes, we test our
  // implementation by making each part that size.
  string initial_str(MockClient::kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(
      initial_str);

  vector<string> strings{string(MockClient::kUploadSize, 'b'),
                         string(MockClient::kUploadSize, 'c')};

  ExpectResumableUpload(*mock_client_, kBucketName, kBlobName, initial_str,
                        strings, true /*expect_queries*/);

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_THAT(async_client.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());
  // After this point, the client is waiting for the context to be done, which
  // it is not.

  // Wait until the stream has been suspended.
  sleep_for(milliseconds(50));
  auto request2 = *put_blob_stream_context_.request;
  request2.mutable_blob_portion()->set_data(strings[0]);
  put_blob_stream_context_.TryPushRequest(move(request2));

  // Wait until the stream has been suspended
  sleep_for(milliseconds(50));
  auto request3 = *put_blob_stream_context_.request;
  request3.mutable_blob_portion()->set_data(strings[1]);
  put_blob_stream_context_.TryPushRequest(move(request3));

  put_blob_stream_context_.MarkDone();

  WaitUntil([this]() { return finish_called_.load(); });

  io_async_executor->Stop();
  cpu_async_executor->Stop();
}

TEST_F(GcpCloudStorageClientProviderStreamTest,
       PutBlobStreamFailsIfInitialWriteFails) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str(MockClient::kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // No additional request objects.
  put_blob_stream_context_.MarkDone();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(Status(CloudStatusCode::kResourceExhausted, "fail")));

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpCloudStorageClientProviderStreamTest,
       PutBlobStreamFailsIfSubsequentWriteFails) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str(MockClient::kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // Place another request on the context.
  put_blob_stream_context_.TryPushRequest(*put_blob_stream_context_.request);
  put_blob_stream_context_.MarkDone();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}))
      .WillOnce(Return(Status(CloudStatusCode::kResourceExhausted, "fail")));

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpCloudStorageClientProviderStreamTest,
       PutBlobStreamFailsIfFinalizingFails) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str(MockClient::kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // Place another request on the context.
  put_blob_stream_context_.TryPushRequest(*put_blob_stream_context_.request);
  put_blob_stream_context_.MarkDone();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}))
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length() * 2, std::nullopt}))
      .WillOnce(Return(Status(CloudStatusCode::kInternal, "fail")));

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result, ResultIs(RetryExecutionResult(
                                    SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpCloudStorageClientProviderStreamTest,
       PutBlobStreamFailsIfStreamExpires) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);
  *put_blob_stream_context_.request->mutable_stream_keepalive_duration() =
      TimeUtil::MicrosecondsToDuration(kStreamKeepAliveMicrosCount);

  string bytes_str(MockClient::kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // Don't mark the context as done and don't enqueue any messages.

  InSequence seq;
  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}));
  EXPECT_CALL(*mock_client_, DeleteResumableUpload);

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());

  sleep_for(microseconds(kStreamKeepAliveMicrosCount));
  EXPECT_TRUE(finish_called_.load());
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpCloudStorageClientProviderStreamTest, PutBlobStreamFailsIfCancelled) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str(MockClient::kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // No additional request objects.
  put_blob_stream_context_.TryCancel();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}));
  EXPECT_CALL(*mock_client_, DeleteResumableUpload);

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_cloud_storage_client_.PutBlobStream(put_blob_stream_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

}  // namespace
}  // namespace google::scp::cpio::client_providers::test
