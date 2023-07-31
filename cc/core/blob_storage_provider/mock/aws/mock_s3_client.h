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

namespace google::scp::core::blob_storage_provider::aws::mock {
class MockS3Client : public Aws::S3::S3Client {
 public:
  std::function<void(
      const Aws::S3::Model::GetObjectRequest&,
      const Aws::S3::GetObjectResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      get_object_async_mock;

  std::function<void(
      const Aws::S3::Model::ListObjectsRequest&,
      const Aws::S3::ListObjectsResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      list_objects_async_mock;

  std::function<void(
      const Aws::S3::Model::PutObjectRequest&,
      const Aws::S3::PutObjectResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      put_object_async_mock;

  std::function<void(
      const Aws::S3::Model::DeleteObjectRequest&,
      const Aws::S3::DeleteObjectResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      delete_object_async_mock;

  void GetObjectAsync(
      const Aws::S3::Model::GetObjectRequest& request,
      const Aws::S3::GetObjectResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (get_object_async_mock) {
      get_object_async_mock(request, handler, context);
      return;
    }

    S3Client::GetObjectAsync(request, handler, context);
  }

  void ListObjectsAsync(
      const Aws::S3::Model::ListObjectsRequest& request,
      const Aws::S3::ListObjectsResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (list_objects_async_mock) {
      list_objects_async_mock(request, handler, context);
      return;
    }

    S3Client::ListObjectsAsync(request, handler, context);
  }

  void PutObjectAsync(
      const Aws::S3::Model::PutObjectRequest& request,
      const Aws::S3::PutObjectResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (put_object_async_mock) {
      put_object_async_mock(request, handler, context);
      return;
    }

    S3Client::PutObjectAsync(request, handler, context);
  }

  void DeleteObjectAsync(
      const Aws::S3::Model::DeleteObjectRequest& request,
      const Aws::S3::DeleteObjectResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (delete_object_async_mock) {
      delete_object_async_mock(request, handler, context);
      return;
    }

    S3Client::DeleteObjectAsync(request, handler, context);
  }
};
}  // namespace google::scp::core::blob_storage_provider::aws::mock
