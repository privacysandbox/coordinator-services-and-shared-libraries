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

#include "cpio/client_providers/role_credentials_provider/src/aws/sts_error_converter.h"

#include <gtest/gtest.h>

#include "cpio/common/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::STS::STSErrors;
using google::scp::core::FailureExecutionResult;

using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::ResultIs;

namespace google::scp::cpio::client_providers::test {
TEST(STSErrorConverter, SucceededToConvertHandledSTSErrors) {
  EXPECT_THAT(
      STSErrorConverter::ConvertSTSError(STSErrors::VALIDATION, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_VALIDATION_FAILED)));
  EXPECT_THAT(
      STSErrorConverter::ConvertSTSError(STSErrors::ACCESS_DENIED, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  STSErrors::INVALID_PARAMETER_COMBINATION, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  STSErrors::INVALID_QUERY_PARAMETER, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  STSErrors::INVALID_PARAMETER_VALUE, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(
      STSErrorConverter::ConvertSTSError(STSErrors::INTERNAL_FAILURE, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(STSErrors::SERVICE_UNAVAILABLE,
                                                 "error"),
              ResultIs(FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(STSErrors::NETWORK_CONNECTION,
                                                 "error"),
              ResultIs(FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)));
  EXPECT_THAT(
      STSErrorConverter::ConvertSTSError(STSErrors::THROTTLING, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED)));
}

TEST(STSErrorConverter, SucceededToConvertNonHandledSTSErrors) {
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  STSErrors::MALFORMED_QUERY_STRING, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
}
}  // namespace google::scp::cpio::client_providers::test
