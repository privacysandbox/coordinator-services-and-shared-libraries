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

#include <gmock/gmock.h>

#include <memory>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/PutObjectRequest.h>

namespace google::scp::cpio::client_providers::mock {

// Even though this is a mock, the default constructor also calls the default
// constructor of S3Client which MUST be called after InitSDK.
class MockS3Client : public Aws::S3::S3Client {
 public:
  MOCK_METHOD(void, GetObjectAsync,
              (const Aws::S3::Model::GetObjectRequest&,
               const Aws::S3::GetObjectResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, ListObjectsAsync,
              (const Aws::S3::Model::ListObjectsRequest&,
               const Aws::S3::ListObjectsResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, PutObjectAsync,
              (const Aws::S3::Model::PutObjectRequest&,
               const Aws::S3::PutObjectResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, DeleteObjectAsync,
              (const Aws::S3::Model::DeleteObjectRequest&,
               const Aws::S3::DeleteObjectResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, CreateMultipartUploadAsync,
              (const Aws::S3::Model::CreateMultipartUploadRequest&,
               const Aws::S3::CreateMultipartUploadResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, UploadPartAsync,
              (const Aws::S3::Model::UploadPartRequest&,
               const Aws::S3::UploadPartResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, CompleteMultipartUploadAsync,
              (const Aws::S3::Model::CompleteMultipartUploadRequest&,
               const Aws::S3::CompleteMultipartUploadResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, AbortMultipartUploadAsync,
              (const Aws::S3::Model::AbortMultipartUploadRequest&,
               const Aws::S3::AbortMultipartUploadResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));
};

}  // namespace google::scp::cpio::client_providers::mock
