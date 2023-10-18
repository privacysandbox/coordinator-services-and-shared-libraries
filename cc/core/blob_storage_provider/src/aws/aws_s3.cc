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

#include "aws_s3.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/PutObjectRequest.h>

#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/blob_storage_provider/src/aws/aws_s3_utils.h"
#include "core/interface/configuration_keys.h"
#include "core/utils/src/base64.h"
#include "core/utils/src/hashing.h"

using Aws::MakeShared;
using Aws::String;
using Aws::StringStream;
using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::S3::S3Client;
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
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::blob_storage_provider::AwsS3Utils;
using google::scp::core::utils::Base64Encode;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

static constexpr char kAwsS3Provider[] = "AwsS3Provider";
static constexpr size_t kMaxConcurrentConnections = 1000;

namespace google::scp::core::blob_storage_provider {
ExecutionResult AwsS3Provider::CreateClientConfig() noexcept {
  client_config_ = make_shared<ClientConfiguration>();
  client_config_->maxConnections = kMaxConcurrentConnections;
  client_config_->executor = make_shared<AwsAsyncExecutor>(io_async_executor_);

  string region;
  auto execution_result = config_provider_->Get(kCloudServiceRegion, region);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  String aws_region(region);
  client_config_->region = aws_region;
  return SuccessExecutionResult();
}

void AwsS3Provider::CreateS3() noexcept {
  s3_client_ = make_shared<S3Client>(*client_config_);
}

ExecutionResult AwsS3Provider::Init() noexcept {
  auto execution_result = CreateClientConfig();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  CreateS3();
  return SuccessExecutionResult();
}

ExecutionResult AwsS3Provider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsS3Provider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsS3Provider::CreateBlobStorageClient(
    shared_ptr<BlobStorageClientInterface>& blob_storage_client) noexcept {
  blob_storage_client = make_shared<AwsS3Client>(s3_client_, async_executor_);
  return SuccessExecutionResult();
}

ExecutionResult AwsS3Client::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  String bucket_name(*get_blob_context.request->bucket_name);
  String blob_name(*get_blob_context.request->blob_name);

  GetObjectRequest get_object_request;
  get_object_request.SetBucket(bucket_name);
  get_object_request.SetKey(blob_name);

  s3_client_->GetObjectAsync(get_object_request,
                             bind(&AwsS3Client::OnGetObjectCallback, this,
                                  get_blob_context, _1, _2, _3, _4),
                             nullptr);

  return SuccessExecutionResult();
}

void AwsS3Client::OnGetObjectCallback(
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context,
    const S3Client* s3_client, const GetObjectRequest& get_object_request,
    const GetObjectOutcome& get_object_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!get_object_outcome.IsSuccess()) {
    SCP_DEBUG_CONTEXT(kAwsS3Provider, get_blob_context,
                      "AwsS3Provider get blob request failed. Error code: %d, "
                      "message: %s",
                      get_object_outcome.GetError().GetResponseCode(),
                      get_object_outcome.GetError().GetMessage().c_str());

    get_blob_context.result = AwsS3Utils::ConvertS3ErrorToExecutionResult(
        get_object_outcome.GetError().GetErrorType());
    if (!async_executor_
             ->Schedule(
                 [get_blob_context]() mutable { get_blob_context.Finish(); },
                 AsyncPriority::High)
             .Successful()) {
      get_blob_context.Finish();
    }
    return;
  }

  // Existing issue with AWS S3 async API.
  // https://github.com/aws/aws-sdk-cpp/issues/918
  auto result = const_cast<GetObjectResult*>(&(get_object_outcome.GetResult()));
  auto& body = result->GetBody();
  auto content_length = result->GetContentLength();

  get_blob_context.response = make_shared<GetBlobResponse>();
  get_blob_context.response->buffer = make_shared<BytesBuffer>();
  get_blob_context.response->buffer->bytes =
      make_shared<vector<Byte>>(content_length);
  get_blob_context.response->buffer->length = content_length;
  get_blob_context.response->buffer->capacity = content_length;
  get_blob_context.result = SuccessExecutionResult();

  if (!body.read(get_blob_context.response->buffer->bytes->data(),
                 content_length)) {
    get_blob_context.result = FailureExecutionResult(
        errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB);
  }

  if (!async_executor_
           ->Schedule(
               [get_blob_context]() mutable { get_blob_context.Finish(); },
               AsyncPriority::High)
           .Successful()) {
    get_blob_context.Finish();
  }
}

ExecutionResult AwsS3Client::ListBlobs(
    AsyncContext<ListBlobsRequest, ListBlobsResponse>&
        list_blobs_context) noexcept {
  String bucket_name(*list_blobs_context.request->bucket_name);

  ListObjectsRequest list_objects_request;
  list_objects_request.SetBucket(bucket_name);

  if (list_blobs_context.request->blob_name) {
    String blob_name(*list_blobs_context.request->blob_name);
    list_objects_request.SetPrefix(blob_name);
  }

  if (list_blobs_context.request->marker) {
    String marker(*list_blobs_context.request->marker);
    list_objects_request.SetMarker(marker);
  }

  s3_client_->ListObjectsAsync(list_objects_request,
                               bind(&AwsS3Client::OnListObjectsCallback, this,
                                    list_blobs_context, _1, _2, _3, _4),
                               nullptr);

  return SuccessExecutionResult();
}

void AwsS3Client::OnListObjectsCallback(
    AsyncContext<ListBlobsRequest, ListBlobsResponse>& list_blobs_context,
    const S3Client* s3_client, const ListObjectsRequest& list_objects_request,
    const ListObjectsOutcome& list_objects_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!list_objects_outcome.IsSuccess()) {
    SCP_DEBUG_CONTEXT(
        kAwsS3Provider, list_blobs_context,
        "AwsS3Provider list blobs request failed. Error code: %d, "
        "message: %s",
        list_objects_outcome.GetError().GetResponseCode(),
        list_objects_outcome.GetError().GetMessage().c_str());
    list_blobs_context.result = AwsS3Utils::ConvertS3ErrorToExecutionResult(
        list_objects_outcome.GetError().GetErrorType());
    if (!async_executor_
             ->Schedule(
                 [list_blobs_context]() mutable {
                   list_blobs_context.Finish();
                 },
                 AsyncPriority::High)
             .Successful()) {
      list_blobs_context.Finish();
    }
    return;
  }

  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  for (auto& object : list_objects_outcome.GetResult().GetContents()) {
    Blob blob;
    blob.blob_name = make_shared<string>(object.GetKey());
    blob.bucket_name = list_blobs_context.request->bucket_name;

    list_blobs_context.response->blobs->push_back(blob);
  }

  Blob next_marker;
  next_marker.blob_name = make_shared<string>(
      list_objects_outcome.GetResult().GetNextMarker().c_str());
  next_marker.bucket_name = list_blobs_context.request->bucket_name;
  list_blobs_context.response->next_marker =
      make_shared<Blob>(move(next_marker));

  list_blobs_context.result = SuccessExecutionResult();
  if (!async_executor_
           ->Schedule(
               [list_blobs_context]() mutable { list_blobs_context.Finish(); },
               AsyncPriority::High)
           .Successful()) {
    list_blobs_context.Finish();
  }
}

ExecutionResult AwsS3Client::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) noexcept {
  if (!put_blob_context.request->bucket_name ||
      !put_blob_context.request->blob_name ||
      put_blob_context.request->bucket_name->empty() ||
      put_blob_context.request->blob_name->empty() ||
      put_blob_context.request->buffer == nullptr) {
    return FailureExecutionResult(
        errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
  }

  String bucket_name(*put_blob_context.request->bucket_name);
  String blob_name(*put_blob_context.request->blob_name);

  PutObjectRequest put_object_request;
  put_object_request.SetBucket(bucket_name);
  put_object_request.SetKey(blob_name);

  ASSIGN_OR_LOG_AND_RETURN_CONTEXT(
      string md5_checksum,
      utils::CalculateMd5Hash(*put_blob_context.request->buffer),
      kAwsS3Provider, put_blob_context, "MD5 Hash generation failed");

  string base64_md5_checksum;
  auto execution_result = Base64Encode(md5_checksum, base64_md5_checksum);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsS3Provider, put_blob_context, execution_result,
                      "Encoding MD5 to base64 failed");
    return execution_result;
  }

  auto input_data = Aws::MakeShared<Aws::StringStream>(
      "PutObjectInputStream", std::stringstream::in | std::stringstream::out |
                                  std::stringstream::binary);
  input_data->write(put_blob_context.request->buffer->bytes->data(),
                    put_blob_context.request->buffer->length);

  put_object_request.SetBody(input_data);
  put_object_request.SetContentMD5(base64_md5_checksum.c_str());

  s3_client_->PutObjectAsync(put_object_request,
                             bind(&AwsS3Client::OnPutObjectCallback, this,
                                  put_blob_context, _1, _2, _3, _4),
                             nullptr);

  return SuccessExecutionResult();
}

void AwsS3Client::OnPutObjectCallback(
    AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context,
    const S3Client* s3_client, const PutObjectRequest& put_object_request,
    const PutObjectOutcome& put_object_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!put_object_outcome.IsSuccess()) {
    SCP_DEBUG_CONTEXT(kAwsS3Provider, put_blob_context,
                      "AwsS3Provider put blob request failed. Error code: %d, "
                      "message: %s",
                      put_object_outcome.GetError().GetResponseCode(),
                      put_object_outcome.GetError().GetMessage().c_str());
    put_blob_context.result = AwsS3Utils::ConvertS3ErrorToExecutionResult(
        put_object_outcome.GetError().GetErrorType());
    if (!async_executor_
             ->Schedule(
                 [put_blob_context]() mutable { put_blob_context.Finish(); },
                 AsyncPriority::High)
             .Successful()) {
      put_blob_context.Finish();
    }
    return;
  }

  put_blob_context.result = SuccessExecutionResult();
  if (!async_executor_
           ->Schedule(
               [put_blob_context]() mutable { put_blob_context.Finish(); },
               AsyncPriority::High)
           .Successful()) {
    put_blob_context.Finish();
  }
}

ExecutionResult AwsS3Client::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
        delete_blob_context) noexcept {
  String bucket_name(*delete_blob_context.request->bucket_name);
  String blob_name(*delete_blob_context.request->blob_name);

  DeleteObjectRequest delete_object_request;
  delete_object_request.SetBucket(bucket_name);
  delete_object_request.SetKey(blob_name);

  s3_client_->DeleteObjectAsync(delete_object_request,
                                bind(&AwsS3Client::OnDeleteObjectCallback, this,
                                     delete_blob_context, _1, _2, _3, _4),
                                nullptr);

  return SuccessExecutionResult();
}

void AwsS3Client::OnDeleteObjectCallback(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& delete_blob_context,
    const S3Client* s3_client, const DeleteObjectRequest& delete_object_request,
    const DeleteObjectOutcome& delete_object_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!delete_object_outcome.IsSuccess()) {
    SCP_DEBUG_CONTEXT(
        kAwsS3Provider, delete_blob_context,
        "AwsS3Provider delete blob request failed. Error code: %d, "
        "message: %s",
        delete_object_outcome.GetError().GetResponseCode(),
        delete_object_outcome.GetError().GetMessage().c_str());
    delete_blob_context.result = AwsS3Utils::ConvertS3ErrorToExecutionResult(
        delete_object_outcome.GetError().GetErrorType());
    if (!async_executor_
             ->Schedule(
                 [delete_blob_context]() mutable {
                   delete_blob_context.Finish();
                 },
                 AsyncPriority::High)
             .Successful()) {
      delete_blob_context.Finish();
    }
    return;
  }

  delete_blob_context.result = SuccessExecutionResult();
  if (!async_executor_
           ->Schedule(
               [delete_blob_context]() mutable {
                 delete_blob_context.Finish();
               },
               AsyncPriority::High)
           .Successful()) {
    delete_blob_context.Finish();
  }
}

}  // namespace google::scp::core::blob_storage_provider
