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

#include <functional>
#include <memory>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/PutObjectRequest.h>

#include "core/blob_storage_provider/src/aws/aws_s3.h"

namespace google::scp::core::blob_storage_provider::mock {
class MockAwsS3Client : public AwsS3Client {
 public:
  explicit MockAwsS3Client(
      std::shared_ptr<Aws::S3::S3Client>& s3_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor)
      : AwsS3Client(s3_client, async_executor) {}

  std::function<void(
      AsyncContext<GetBlobRequest, GetBlobResponse>&, const Aws::S3::S3Client*,
      const Aws::S3::Model::GetObjectRequest&,
      const Aws::S3::Model::GetObjectOutcome&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>)>
      on_get_object_callback_mock;

  std::function<void(
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&,
      const Aws::S3::S3Client*, const Aws::S3::Model::ListObjectsRequest&,
      const Aws::S3::Model::ListObjectsOutcome&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>)>
      on_list_objects_callback_mock;

  std::function<void(
      AsyncContext<PutBlobRequest, PutBlobResponse>&, const Aws::S3::S3Client*,
      const Aws::S3::Model::PutObjectRequest&,
      const Aws::S3::Model::PutObjectOutcome&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>)>
      on_put_object_callback_mock;

  std::function<void(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&,
      const Aws::S3::S3Client*, const Aws::S3::Model::DeleteObjectRequest&,
      const Aws::S3::Model::DeleteObjectOutcome&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>)>
      on_delete_object_callback_mock;

  void OnGetObjectCallback(
      AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::GetObjectRequest& get_object_request,
      const Aws::S3::Model::GetObjectOutcome& get_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    if (on_get_object_callback_mock) {
      on_get_object_callback_mock(get_blob_context, s3_client,
                                  get_object_request, get_object_outcome,
                                  async_context);
      return;
    }

    AwsS3Client::OnGetObjectCallback(get_blob_context, s3_client,
                                     get_object_request, get_object_outcome,
                                     async_context);
  }

  void OnListObjectsCallback(
      AsyncContext<ListBlobsRequest, ListBlobsResponse>& list_objects_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::ListObjectsRequest& list_objects_request,
      const Aws::S3::Model::ListObjectsOutcome& list_objects_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    if (on_list_objects_callback_mock) {
      on_list_objects_callback_mock(list_objects_context, s3_client,
                                    list_objects_request, list_objects_outcome,
                                    async_context);
      return;
    }

    AwsS3Client::OnListObjectsCallback(list_objects_context, s3_client,
                                       list_objects_request,
                                       list_objects_outcome, async_context);
  }

  void OnPutObjectCallback(
      AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::PutObjectRequest& put_object_request,
      const Aws::S3::Model::PutObjectOutcome& put_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    if (on_put_object_callback_mock) {
      on_put_object_callback_mock(put_blob_context, s3_client,
                                  put_object_request, put_object_outcome,
                                  async_context);
      return;
    }

    AwsS3Client::OnPutObjectCallback(put_blob_context, s3_client,
                                     put_object_request, put_object_outcome,
                                     async_context);
  }

  void OnDeleteObjectCallback(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& delete_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::DeleteObjectRequest& delete_object_request,
      const Aws::S3::Model::DeleteObjectOutcome& delete_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    if (on_delete_object_callback_mock) {
      on_delete_object_callback_mock(delete_blob_context, s3_client,
                                     delete_object_request,
                                     delete_object_outcome, async_context);
      return;
    }

    AwsS3Client::OnDeleteObjectCallback(delete_blob_context, s3_client,
                                        delete_object_request,
                                        delete_object_outcome, async_context);
  }
};
}  // namespace google::scp::core::blob_storage_provider::mock
