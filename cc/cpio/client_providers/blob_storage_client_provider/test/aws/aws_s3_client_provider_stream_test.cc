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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/CompletedMultipartUpload.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/Object.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <google/protobuf/util/time_util.h>

#include "absl/strings/str_cat.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/blob_storage_client_provider/src/aws/aws_s3_client_provider.h"
#include "cpio/client_providers/blob_storage_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/blob_storage_client_provider/test/aws/mock_s3_client.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::StringStream;
using Aws::Vector;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using Aws::S3::S3Client;
using Aws::S3::S3Errors;
using Aws::S3::Model::AbortMultipartUploadOutcome;
using Aws::S3::Model::AbortMultipartUploadRequest;
using Aws::S3::Model::AbortMultipartUploadResult;
using Aws::S3::Model::CompletedMultipartUpload;
using Aws::S3::Model::CompletedPart;
using Aws::S3::Model::CompleteMultipartUploadOutcome;
using Aws::S3::Model::CompleteMultipartUploadRequest;
using Aws::S3::Model::CompleteMultipartUploadResult;
using Aws::S3::Model::CreateMultipartUploadOutcome;
using Aws::S3::Model::CreateMultipartUploadRequest;
using Aws::S3::Model::CreateMultipartUploadResult;
using Aws::S3::Model::GetObjectOutcome;
using Aws::S3::Model::GetObjectRequest;
using Aws::S3::Model::GetObjectResult;
using Aws::S3::Model::Object;
using Aws::S3::Model::UploadPartOutcome;
using Aws::S3::Model::UploadPartRequest;
using Aws::S3::Model::UploadPartResult;
using google::cmrt::sdk::blob_storage_service::v1::Blob;
using google::cmrt::sdk::blob_storage_service::v1::BlobMetadata;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncExecutor;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED;
using google::scp::core::errors::SC_STREAMING_CONTEXT_DONE;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockS3Client;
using std::atomic_bool;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::NiceMock;
using testing::Pointwise;
using testing::Return;
using testing::UnorderedPointwise;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kBucketName[] = "bucket";
constexpr char kBlobName[] = "blob";

constexpr size_t kMinimumPartSize = 5 << 20;
constexpr int64_t kStreamKeepAliveMicrosCount = 100;
}  // namespace

namespace google::scp::cpio::client_providers {

class MockAwsS3Factory : public AwsS3Factory {
 public:
  MOCK_METHOD(core::ExecutionResultOr<shared_ptr<Aws::S3::S3Client>>,
              CreateClient,
              (ClientConfiguration&,
               const shared_ptr<core::AsyncExecutorInterface>&),
              (noexcept, override));
};

class AwsS3ClientProviderStreamTest : public ::testing::Test {
 protected:
  AwsS3ClientProviderStreamTest()
      : instance_client_(make_shared<MockInstanceClientProvider>()),
        s3_factory_(make_shared<NiceMock<MockAwsS3Factory>>()),
        provider_(make_shared<BlobStorageClientOptions>(), instance_client_,
                  make_shared<MockAsyncExecutor>(),
                  make_shared<MockAsyncExecutor>(), s3_factory_) {
    InitAPI(options_);
    instance_client_->instance_resource_name = kResourceNameMock;
    s3_client_ = make_shared<NiceMock<MockS3Client>>();
    abstract_client_ = static_cast<S3Client*>(s3_client_.get());

    ON_CALL(*s3_factory_, CreateClient).WillByDefault(Return(s3_client_));

    put_blob_stream_context_.request = make_shared<PutBlobStreamRequest>();
    put_blob_stream_context_.callback = [this](auto) { finish_called_ = true; };

    get_blob_stream_context_.request = make_shared<GetBlobStreamRequest>();
    get_blob_stream_context_.process_callback = [this](auto, auto) {
      finish_called_ = true;
    };

    EXPECT_SUCCESS(provider_.Init());
    EXPECT_SUCCESS(provider_.Run());
  }

  ~AwsS3ClientProviderStreamTest() { ShutdownAPI(options_); }

  shared_ptr<MockInstanceClientProvider> instance_client_;
  shared_ptr<MockS3Client> s3_client_;
  S3Client* abstract_client_;
  shared_ptr<MockAwsS3Factory> s3_factory_;
  AwsS3ClientProvider provider_;

  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
      put_blob_stream_context_;

  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
      get_blob_stream_context_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  atomic_bool finish_called_{false};

  SDKOptions options_;
};

MATCHER_P2(HasBucketAndKey, bucket, key, "") {
  return ExplainMatchResult(Eq(bucket), arg.GetBucket(), result_listener) &&
         ExplainMatchResult(Eq(key), arg.GetKey(), result_listener);
}

///////////// PutBlobStream ///////////////////////////////////////////////////

MATCHER_P5(UploadPartRequestEquals, bucket_name, key, upload_id, part_number,
           body, "") {
  string body_string(body.length(), 0);
  arg.GetBody()->read(body_string.data(), body_string.length());
  return ExplainMatchResult(HasBucketAndKey(bucket_name, key), arg,
                            result_listener) &&
         ExplainMatchResult(Eq(upload_id), arg.GetUploadId(),
                            result_listener) &&
         ExplainMatchResult(Eq(part_number), arg.GetPartNumber(),
                            result_listener) &&
         ExplainMatchResult(Eq(body), body_string, result_listener);
}

CompletedPart MakeCompletedPart(const string& etag, int64_t part_number) {
  return CompletedPart().WithETag(etag.c_str()).WithPartNumber(part_number);
}

MATCHER(CompletedPartEquals, "") {
  const auto& [actual, expected] = arg;
  return ExplainMatchResult(Eq(expected.GetETag()), actual.GetETag(),
                            result_listener) &&
         ExplainMatchResult(Eq(expected.GetPartNumber()),
                            actual.GetPartNumber(), result_listener);
}

MATCHER_P3(HasBucketKeyAndUpload, bucket_name, blob_name, upload, "") {
  return ExplainMatchResult(HasBucketAndKey(bucket_name, blob_name), arg,
                            result_listener) &&
         ExplainMatchResult(
             UnorderedPointwise(CompletedPartEquals(), upload.GetParts()),
             arg.GetMultipartUpload().GetParts(), result_listener);
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStream) {
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

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_NE(context.response, nullptr);

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id);
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  string etag = "tag 1";

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 1, bytes_str),
                              _, _))
      .WillOnce([this, &etag](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  CompletedMultipartUpload upload;
  upload.AddParts(MakeCompletedPart(etag, 1));

  EXPECT_CALL(*s3_client_,
              CompleteMultipartUploadAsync(
                  HasBucketKeyAndUpload(kBucketName, kBlobName, upload), _, _))
      .WillOnce([this](auto request, auto& callback, auto) {
        CompleteMultipartUploadResult result;
        CompleteMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, outcome, nullptr);
      });

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStreamMultiplePortions) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str(kMinimumPartSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);

  vector<string> strings{string(kMinimumPartSize, 'b'),
                         string(kMinimumPartSize, 'c')};
  auto request2 = *put_blob_stream_context_.request;
  request2.mutable_blob_portion()->set_data(strings[0]);
  auto request3 = *put_blob_stream_context_.request;
  request3.mutable_blob_portion()->set_data(strings[1]);
  put_blob_stream_context_.TryPushRequest(move(request2));
  put_blob_stream_context_.TryPushRequest(move(request3));
  put_blob_stream_context_.MarkDone();

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id);
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  string etag1 = "tag 1", etag2 = "tag 2", etag3 = "tag 3";

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 1, bytes_str),
                              _, _))
      .WillOnce([this, &etag1](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag1);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 2, strings[0]),
                              _, _))
      .WillOnce([this, &etag2](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag2);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 3, strings[1]),
                              _, _))
      .WillOnce([this, &etag3](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag3);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  CompletedMultipartUpload upload;
  upload.AddParts(MakeCompletedPart(etag1, 1));
  upload.AddParts(MakeCompletedPart(etag2, 2));
  upload.AddParts(MakeCompletedPart(etag3, 3));

  EXPECT_CALL(*s3_client_,
              CompleteMultipartUploadAsync(
                  HasBucketKeyAndUpload(kBucketName, kBlobName, upload), _, _))
      .WillOnce([this](auto request, auto& callback, auto) {
        CompleteMultipartUploadResult result;
        CompleteMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, outcome, nullptr);
      });

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStreamAccumulates) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str = "initial";
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);

  // With this setup, we would expect "initialnext oneaaaaa..." to be one
  // UploadPart and "final one" be another.
  vector<string> strings{"next one", string(kMinimumPartSize, 'a'),
                         "final one"};
  auto request2 = *put_blob_stream_context_.request;
  request2.mutable_blob_portion()->set_data(strings[0]);
  auto request3 = *put_blob_stream_context_.request;
  request3.mutable_blob_portion()->set_data(strings[1]);
  auto request4 = *put_blob_stream_context_.request;
  request4.mutable_blob_portion()->set_data(strings[2]);
  put_blob_stream_context_.TryPushRequest(move(request2));
  put_blob_stream_context_.TryPushRequest(move(request3));
  put_blob_stream_context_.TryPushRequest(move(request4));
  put_blob_stream_context_.MarkDone();

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id);
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  string etag1 = "tag 1", etag2 = "tag 2";

  string expected_accumulated_string = bytes_str + strings[0] + strings[1];
  EXPECT_CALL(
      *s3_client_,
      UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName, upload_id,
                                              1, expected_accumulated_string),
                      _, _))
      .WillOnce([this, &etag1](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag1);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 2, strings[2]),
                              _, _))
      .WillOnce([this, &etag2](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag2);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  CompletedMultipartUpload upload;
  upload.AddParts(MakeCompletedPart(etag1, 1));
  upload.AddParts(MakeCompletedPart(etag2, 2));

  EXPECT_CALL(*s3_client_,
              CompleteMultipartUploadAsync(
                  HasBucketKeyAndUpload(kBucketName, kBlobName, upload), _, _))
      .WillOnce([this](auto request, auto& callback, auto) {
        CompleteMultipartUploadResult result;
        CompleteMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, outcome, nullptr);
      });

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsS3ClientProviderStreamTest,
       PutBlobStreamMultiplePortionsWithNoOpCycles) {
  // In order to test the "no message" path, we must have real async executors.
  auto cpu_async_executor = make_shared<AsyncExecutor>(2, 10),
       io_async_executor = make_shared<AsyncExecutor>(2, 10);
  EXPECT_SUCCESS(cpu_async_executor->Init());
  EXPECT_SUCCESS(io_async_executor->Init());
  EXPECT_SUCCESS(cpu_async_executor->Run());
  EXPECT_SUCCESS(io_async_executor->Run());
  AwsS3ClientProvider async_client(make_shared<BlobStorageClientOptions>(),
                                   instance_client_, cpu_async_executor,
                                   io_async_executor, s3_factory_);
  EXPECT_SUCCESS(async_client.Init());
  EXPECT_SUCCESS(async_client.Run());
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str(kMinimumPartSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);

  vector<string> strings{string(kMinimumPartSize, 'b'),
                         string(kMinimumPartSize, 'c')};
  auto request2 = *put_blob_stream_context_.request;
  request2.mutable_blob_portion()->set_data(strings[0]);
  auto request3 = *put_blob_stream_context_.request;
  request3.mutable_blob_portion()->set_data(strings[1]);
  put_blob_stream_context_.TryPushRequest(move(request2));
  put_blob_stream_context_.TryPushRequest(move(request3));
  put_blob_stream_context_.MarkDone();

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id);
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  string etag1 = "tag 1", etag2 = "tag 2", etag3 = "tag 3";

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 1, bytes_str),
                              _, _))
      .WillOnce([this, &etag1](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag1);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 2, strings[0]),
                              _, _))
      .WillOnce([this, &etag2](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag2);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 3, strings[1]),
                              _, _))
      .WillOnce([this, &etag3](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag3);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  CompletedMultipartUpload upload;
  upload.AddParts(MakeCompletedPart(etag1, 1));
  upload.AddParts(MakeCompletedPart(etag2, 2));
  upload.AddParts(MakeCompletedPart(etag3, 3));

  EXPECT_CALL(*s3_client_,
              CompleteMultipartUploadAsync(
                  HasBucketKeyAndUpload(kBucketName, kBlobName, upload), _, _))
      .WillOnce([this](auto request, auto& callback, auto) {
        CompleteMultipartUploadResult result;
        CompleteMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, outcome, nullptr);
      });

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));
  // After this point, the client is waiting for the context to be done, which
  // it is not.

  // Wait until it enters the code path we want.
  sleep_for(milliseconds(50));
  put_blob_stream_context_.TryPushRequest(move(request2));

  // Wait until it enters the code path we want.
  sleep_for(milliseconds(50));
  put_blob_stream_context_.TryPushRequest(move(request3));

  put_blob_stream_context_.MarkDone();

  WaitUntil([this]() { return finish_called_.load(); });

  io_async_executor->Stop();
  cpu_async_executor->Stop();
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStreamFailsIfCreateFails) {
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

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));

    finish_called_ = true;
  };

  InSequence in_sequence;

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this](auto request, auto& callback, auto) {
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        CreateMultipartUploadOutcome outcome(move(s3_error));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_, UploadPartAsync).Times(0);

  EXPECT_CALL(*s3_client_, CompleteMultipartUploadAsync).Times(0);

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P3(HasBucketKeyAndUploadId, bucket_name, blob_name, upload_id, "") {
  return ExplainMatchResult(HasBucketAndKey(bucket_name, blob_name), arg,
                            result_listener) &&
         ExplainMatchResult(Eq(upload_id), arg.GetUploadId(), result_listener);
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStreamFailsIfUploadPartFails) {
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

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id.c_str());
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_, UploadPartAsync)
      .WillOnce([this](auto request, auto& callback, auto) {
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        CreateMultipartUploadOutcome outcome(move(s3_error));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(
      *s3_client_,
      AbortMultipartUploadAsync(
          HasBucketKeyAndUploadId(kBucketName, kBlobName, upload_id), _, _))
      .WillOnce([this](auto request, auto& callback, auto) {
        AbortMultipartUploadResult result;
        AbortMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_, CompleteMultipartUploadAsync).Times(0);

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStreamFailsIfCompleteFails) {
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

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id);
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  string etag = "tag 1";

  EXPECT_CALL(*s3_client_,
              UploadPartAsync(UploadPartRequestEquals(kBucketName, kBlobName,
                                                      upload_id, 1, bytes_str),
                              _, _))
      .WillOnce([this, &etag](auto request, auto& callback, auto) {
        UploadPartResult result;
        result.SetETag(etag);
        UploadPartOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  CompletedMultipartUpload upload;
  upload.AddParts(MakeCompletedPart(etag, 1));

  EXPECT_CALL(*s3_client_,
              CompleteMultipartUploadAsync(
                  HasBucketKeyAndUpload(kBucketName, kBlobName, upload), _, _))
      .WillOnce([this](auto request, auto& callback, auto) {
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        CompleteMultipartUploadOutcome outcome(move(s3_error));
        callback(abstract_client_, request, outcome, nullptr);
      });

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStreamFailsIfStreamExpires) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);
  *put_blob_stream_context_.request->mutable_stream_keepalive_duration() =
      TimeUtil::MicrosecondsToDuration(kStreamKeepAliveMicrosCount);

  string bytes_str = "initial";
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // Don't mark the context as done and don't enqueue any messages.

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED)));

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id);
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_, AbortMultipartUploadAsync)
      .WillOnce([this](auto request, auto& callback, auto) {
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        AbortMultipartUploadOutcome outcome(move(s3_error));
        callback(abstract_client_, request, outcome, nullptr);
      });

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  sleep_for(microseconds(kStreamKeepAliveMicrosCount));
  EXPECT_TRUE(finish_called_.load());
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(AwsS3ClientProviderStreamTest, PutBlobStreamFailsIfStreamCancelled) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  string bytes_str = "initial";
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // No additional request objects.
  put_blob_stream_context_.TryCancel();

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED)));

    finish_called_ = true;
  };

  InSequence in_sequence;

  string upload_id = "upload id";

  EXPECT_CALL(*s3_client_, CreateMultipartUploadAsync(
                               HasBucketAndKey(kBucketName, kBlobName), _, _))
      .WillOnce([this, &upload_id](auto request, auto& callback, auto) {
        CreateMultipartUploadResult result;
        result.SetUploadId(upload_id);
        CreateMultipartUploadOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*s3_client_, AbortMultipartUploadAsync)
      .WillOnce([this](auto request, auto& callback, auto) {
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        AbortMultipartUploadOutcome outcome(move(s3_error));
        callback(abstract_client_, request, outcome, nullptr);
      });

  EXPECT_SUCCESS(provider_.PutBlobStream(put_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

///////////// GetBlobStream ///////////////////////////////////////////////////

MATCHER_P3(HasBucketKeyAndRange, bucket_name, blob_name, range, "") {
  return ExplainMatchResult(HasBucketAndKey(bucket_name, blob_name), arg,
                            result_listener) &&
         ExplainMatchResult(Eq(range), arg.GetRange(), result_listener);
}

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

TEST_F(AwsS3ClientProviderStreamTest, GetBlobStream) {
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

  EXPECT_CALL(
      *s3_client_,
      GetObjectAsync(
          HasBucketKeyAndRange(kBucketName, kBlobName, "bytes=0-65535"), _, _))
      .WillOnce([this, &bytes_str](auto request, auto& callback, auto) {
        GetObjectResult result;
        result.ReplaceBody(new StringStream(bytes_str));
        result.SetContentRange("bytes 0-14/15");
        result.SetContentLength(bytes_str.length());
        GetObjectOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

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

  EXPECT_SUCCESS(provider_.GetBlobStream(get_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              ElementsAre(GetBlobStreamResponseEquals(expected_response)));
}

TEST_F(AwsS3ClientProviderStreamTest, GetBlobStreamMultipleResponses) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.request->set_max_bytes_per_response(2);

  // 15 chars.
  string bytes_str = "response_string";
  // Expect to get responses with data: ["re", "sp", ... "g"]
  vector<GetBlobStreamResponse> expected_responses;
  for (size_t i = 0; i < bytes_str.length(); i += 2) {
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

  InSequence in_sequence;
  for (size_t i = 0; i < bytes_str.length(); i += 2) {
    EXPECT_CALL(*s3_client_,
                GetObjectAsync(
                    HasBucketKeyAndRange(kBucketName, kBlobName,
                                         absl::StrCat("bytes=", i, "-", i + 1)),
                    _, _))
        .WillOnce([this, &bytes_str, i](auto request, auto& callback, auto) {
          auto end_index = std::min(i + 1, bytes_str.length() - 1);
          GetObjectResult result;
          result.ReplaceBody(new StringStream(bytes_str.substr(i, 2)));
          result.SetContentRange(
              absl::StrCat("bytes ", i, "-", end_index, "/15"));
          result.SetContentLength(end_index - i + 1);
          GetObjectOutcome outcome(move(result));
          callback(abstract_client_, request, move(outcome), nullptr);
        });
  }

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

  EXPECT_SUCCESS(provider_.GetBlobStream(get_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              Pointwise(GetBlobStreamResponseEquals(), expected_responses));
}

TEST_F(AwsS3ClientProviderStreamTest, GetBlobStreamByteRange) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.request->set_max_bytes_per_response(3);
  get_blob_stream_context_.request->mutable_byte_range()->set_begin_byte_index(
      3);
  get_blob_stream_context_.request->mutable_byte_range()->set_end_byte_index(6);

  // We slice "response_string" to indices 3-6.
  string bytes_str = "pons";
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

  InSequence in_sequence;
  EXPECT_CALL(
      *s3_client_,
      GetObjectAsync(HasBucketKeyAndRange(kBucketName, kBlobName,
                                          absl::StrCat("bytes=", 3, "-", 5)),
                     _, _))
      .WillOnce([this, &bytes_str](auto request, auto& callback, auto) {
        GetObjectResult result;
        result.ReplaceBody(new StringStream(bytes_str.substr(0, 3)));
        // Set actual length to be 15 so that it thinks there is more object
        // than we're getting.
        result.SetContentRange(absl::StrCat("bytes ", 3, "-", 5, "/15"));
        result.SetContentLength(3);
        GetObjectOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });
  EXPECT_CALL(
      *s3_client_,
      GetObjectAsync(HasBucketKeyAndRange(kBucketName, kBlobName,
                                          absl::StrCat("bytes=", 6, "-", 6)),
                     _, _))
      .WillOnce([this, &bytes_str](auto request, auto& callback, auto) {
        GetObjectResult result;
        result.ReplaceBody(new StringStream(bytes_str.substr(3, 3)));
        // Set actual length to be 15 so that it thinks there is more object
        // than we're getting.
        result.SetContentRange(absl::StrCat("bytes ", 6, "-", 6, "/15"));
        result.SetContentLength(1);
        GetObjectOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

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

  EXPECT_SUCCESS(provider_.GetBlobStream(get_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              Pointwise(GetBlobStreamResponseEquals(), expected_responses));
}

TEST_F(AwsS3ClientProviderStreamTest, GetBlobStreamIndexBeyondEnd) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.request->mutable_byte_range()->set_begin_byte_index(
      0);
  get_blob_stream_context_.request->mutable_byte_range()->set_end_byte_index(
      1000);

  // 15 chars.
  string bytes_str = "response_string";
  GetBlobStreamResponse expected_response;
  expected_response.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      get_blob_stream_context_.request->blob_metadata());
  *expected_response.mutable_blob_portion()->mutable_data() = bytes_str;
  expected_response.mutable_byte_range()->set_begin_byte_index(0);
  expected_response.mutable_byte_range()->set_end_byte_index(14);

  EXPECT_CALL(
      *s3_client_,
      GetObjectAsync(
          HasBucketKeyAndRange(kBucketName, kBlobName, "bytes=0-1000"), _, _))
      .WillOnce([this, &bytes_str](auto request, auto& callback, auto) {
        GetObjectResult result;
        result.ReplaceBody(new StringStream(bytes_str));
        result.SetContentRange("bytes 0-14/15");
        result.SetContentLength(bytes_str.length());
        GetObjectOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

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

  EXPECT_SUCCESS(provider_.GetBlobStream(get_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              ElementsAre(GetBlobStreamResponseEquals(expected_response)));
}

TEST_F(AwsS3ClientProviderStreamTest, GetBlobStreamFailsIfGetObjectFails) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);

  EXPECT_CALL(*s3_client_, GetObjectAsync)
      .WillOnce([this](auto request, auto& callback, auto) {
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        GetObjectOutcome outcome(move(s3_error));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  get_blob_stream_context_.process_callback = [this](auto& context, bool) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
    finish_called_ = true;
  };

  EXPECT_SUCCESS(provider_.GetBlobStream(get_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
}

TEST_F(AwsS3ClientProviderStreamTest, GetBlobStreamFailsIfQueueDone) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.MarkDone();

  // 15 chars.
  string bytes_str = "response_string";

  EXPECT_CALL(
      *s3_client_,
      GetObjectAsync(
          HasBucketKeyAndRange(kBucketName, kBlobName, "bytes=0-65535"), _, _))
      .WillOnce([this, &bytes_str](auto request, auto& callback, auto) {
        GetObjectResult result;
        result.ReplaceBody(new StringStream(bytes_str));
        result.SetContentRange("bytes 0-14/15");
        result.SetContentLength(bytes_str.length());
        GetObjectOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  get_blob_stream_context_.process_callback = [this](auto& context, bool) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_STREAMING_CONTEXT_DONE)));
    finish_called_ = true;
  };

  EXPECT_SUCCESS(provider_.GetBlobStream(get_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
}

TEST_F(AwsS3ClientProviderStreamTest, GetBlobStreamFailsIfRequestCancelled) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.TryCancel();

  // 15 chars.
  string bytes_str = "response_string";

  EXPECT_CALL(
      *s3_client_,
      GetObjectAsync(
          HasBucketKeyAndRange(kBucketName, kBlobName, "bytes=0-65535"), _, _))
      .WillOnce([this, &bytes_str](auto request, auto& callback, auto) {
        GetObjectResult result;
        result.ReplaceBody(new StringStream(bytes_str));
        result.SetContentRange("bytes 0-14/15");
        result.SetContentLength(bytes_str.length());
        GetObjectOutcome outcome(move(result));
        callback(abstract_client_, request, move(outcome), nullptr);
      });

  get_blob_stream_context_.process_callback = [this](auto& context, bool) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED)));
    finish_called_ = true;
  };

  EXPECT_SUCCESS(provider_.GetBlobStream(get_blob_stream_context_));

  WaitUntil([this]() { return finish_called_.load(); });
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
}

}  // namespace google::scp::cpio::client_providers
