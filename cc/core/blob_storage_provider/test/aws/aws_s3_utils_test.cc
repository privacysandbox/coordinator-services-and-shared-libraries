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

#include "core/blob_storage_provider/src/aws/aws_s3_utils.h"

#include <gtest/gtest.h>

#include <aws/s3/S3Errors.h>

#include "core/blob_storage_provider/src/common/error_codes.h"

using Aws::S3::S3Errors;
using google::scp::core::blob_storage_provider::AwsS3Utils;

namespace google::scp::core::test {

TEST(S3DBUtilsTests, ConvertS3ErrorToExecutionResult) {
  // Retriable error codes.
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::INTERNAL_FAILURE),
      RetryExecutionResult(errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::SERVICE_UNAVAILABLE),
      RetryExecutionResult(errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::THROTTLING),
      RetryExecutionResult(errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR));

  // Unretriable error codes
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::INCOMPLETE_SIGNATURE),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::INVALID_ACTION),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::INVALID_CLIENT_TOKEN_ID),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::INVALID_PARAMETER_COMBINATION),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::INVALID_QUERY_PARAMETER),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::INVALID_PARAMETER_VALUE),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::MISSING_ACTION),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::MISSING_AUTHENTICATION_TOKEN),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::MISSING_PARAMETER),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::OPT_IN_REQUIRED),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::REQUEST_EXPIRED),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::VALIDATION),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::ACCESS_DENIED),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::RESOURCE_NOT_FOUND),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::UNRECOGNIZED_CLIENT),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::MALFORMED_QUERY_STRING),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::REQUEST_TIME_TOO_SKEWED),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::INVALID_SIGNATURE),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::SIGNATURE_DOES_NOT_MATCH),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::INVALID_ACCESS_KEY_ID),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NETWORK_CONNECTION),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::UNKNOWN),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::BUCKET_ALREADY_EXISTS),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::BUCKET_ALREADY_OWNED_BY_YOU),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NO_SUCH_BUCKET),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NO_SUCH_KEY),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND));
  EXPECT_EQ(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NO_SUCH_UPLOAD),
      FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::OBJECT_ALREADY_IN_ACTIVE_TIER),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
  EXPECT_EQ(AwsS3Utils::ConvertS3ErrorToExecutionResult(
                S3Errors::OBJECT_NOT_IN_ACTIVE_TIER),
            FailureExecutionResult(
                errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR));
}

}  // namespace google::scp::core::test
