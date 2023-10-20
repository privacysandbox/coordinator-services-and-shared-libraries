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

#include "cpio/client_providers/blob_storage_client_provider/src/aws/aws_s3_utils.h"

#include <gtest/gtest.h>

#include <aws/s3/S3Errors.h>

#include "cpio/common/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::S3::S3Errors;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::test::ResultIs;
namespace errors = google::scp::core::errors;

namespace google::scp::cpio::client_providers::test {

TEST(S3DBUtilsTests, ConvertS3ErrorToExecutionResult) {
  // Retriable error codes.
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::SERVICE_UNAVAILABLE),
      ResultIs(RetryExecutionResult(errors::SC_AWS_SERVICE_UNAVAILABLE)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::THROTTLING),
      ResultIs(RetryExecutionResult(errors::SC_AWS_SERVICE_UNAVAILABLE)));

  // Unretriable error codes
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::INTERNAL_FAILURE),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::INCOMPLETE_SIGNATURE),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::INVALID_ACTION),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::INVALID_CLIENT_TOKEN_ID),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::INVALID_PARAMETER_COMBINATION),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::INVALID_QUERY_PARAMETER),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::INVALID_PARAMETER_VALUE),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::MISSING_ACTION),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::MISSING_AUTHENTICATION_TOKEN),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::MISSING_PARAMETER),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::OPT_IN_REQUIRED),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::REQUEST_EXPIRED),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::VALIDATION),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::ACCESS_DENIED),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::RESOURCE_NOT_FOUND),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::UNRECOGNIZED_CLIENT),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::MALFORMED_QUERY_STRING),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::REQUEST_TIME_TOO_SKEWED),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::INVALID_SIGNATURE),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::SIGNATURE_DOES_NOT_MATCH),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::INVALID_ACCESS_KEY_ID),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NETWORK_CONNECTION),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::UNKNOWN),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::BUCKET_ALREADY_EXISTS),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::BUCKET_ALREADY_OWNED_BY_YOU),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NO_SUCH_BUCKET),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NO_SUCH_KEY),
      ResultIs(FailureExecutionResult(errors::SC_AWS_NOT_FOUND)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(S3Errors::NO_SUCH_UPLOAD),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::OBJECT_ALREADY_IN_ACTIVE_TIER),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(
      AwsS3Utils::ConvertS3ErrorToExecutionResult(
          S3Errors::OBJECT_NOT_IN_ACTIVE_TIER),
      ResultIs(FailureExecutionResult(errors::SC_AWS_INTERNAL_SERVICE_ERROR)));
}

}  // namespace google::scp::cpio::client_providers::test
