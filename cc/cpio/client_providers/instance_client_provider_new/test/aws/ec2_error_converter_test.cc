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

#include "cpio/client_providers/instance_client_provider_new/src/aws/ec2_error_converter.h"

#include <gtest/gtest.h>

#include "cpio/client_providers/instance_client_provider_new/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"

using Aws::EC2::EC2Errors;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;

namespace google::scp::cpio::client_providers::test {
TEST(EC2ErrorConverter, SucceededToConvertHandledEC2Errors) {
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(EC2Errors::VALIDATION, "error"),
            FailureExecutionResult(SC_AWS_VALIDATION_FAILED));
  EXPECT_EQ(
      EC2ErrorConverter::ConvertEC2Error(EC2Errors::ACCESS_DENIED, "error"),
      FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS));
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(
                EC2Errors::INVALID_PARAMETER_COMBINATION, "error"),
            FailureExecutionResult(SC_AWS_INVALID_REQUEST));
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(
                EC2Errors::INVALID_QUERY_PARAMETER, "error"),
            FailureExecutionResult(SC_AWS_INVALID_REQUEST));
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(
                EC2Errors::INVALID_PARAMETER_VALUE, "error"),
            FailureExecutionResult(SC_AWS_INVALID_REQUEST));
  EXPECT_EQ(
      EC2ErrorConverter::ConvertEC2Error(EC2Errors::INTERNAL_FAILURE, "error"),
      FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR));
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(EC2Errors::SERVICE_UNAVAILABLE,
                                               "error"),
            FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE));
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(EC2Errors::NETWORK_CONNECTION,
                                               "error"),
            FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE));
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(EC2Errors::THROTTLING, "error"),
            FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED));
  EXPECT_EQ(
      EC2ErrorConverter::ConvertEC2Error(EC2Errors::INTERNAL_FAILURE, "error"),
      FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR));
}

TEST(EC2ErrorConverter, SucceededToConvertNonHandledEC2Errors) {
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(
                EC2Errors::INVALID_GROUP_ID__MALFORMED, "error"),
            FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR));
  EXPECT_EQ(
      EC2ErrorConverter::ConvertEC2Error(EC2Errors::DRY_RUN_OPERATION, "error"),
      FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR));
  EXPECT_EQ(EC2ErrorConverter::ConvertEC2Error(
                EC2Errors::OPERATION_NOT_PERMITTED, "error"),
            FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR));
}
}  // namespace google::scp::cpio::client_providers::test
