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

#include "cpio/client_providers/parameter_client_provider/src/aws/ssm_error_converter.h"

#include <gtest/gtest.h>

#include "cpio/client_providers/parameter_client_provider/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::SSM::SSMErrors;
using google::scp::core::FailureExecutionResult;

using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::ResultIs;

namespace google::scp::cpio::client_providers::test {
TEST(SSMErrorConverter, SucceededToConvertHandledSSMErrors) {
  EXPECT_THAT(
      SSMErrorConverter::ConvertSSMError(SSMErrors::VALIDATION, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_VALIDATION_FAILED)));
  EXPECT_THAT(
      SSMErrorConverter::ConvertSSMError(SSMErrors::ACCESS_DENIED, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(
                  SSMErrors::INVALID_PARAMETER_COMBINATION, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(
                  SSMErrors::INVALID_QUERY_PARAMETER, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(
                  SSMErrors::INVALID_PARAMETER_VALUE, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(SSMErrors::PARAMETER_NOT_FOUND,
                                                 "error"),
              ResultIs(FailureExecutionResult(
                  SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND)));
  EXPECT_THAT(
      SSMErrorConverter::ConvertSSMError(SSMErrors::INTERNAL_FAILURE, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(SSMErrors::SERVICE_UNAVAILABLE,
                                                 "error"),
              ResultIs(FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(SSMErrors::NETWORK_CONNECTION,
                                                 "error"),
              ResultIs(FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)));
  EXPECT_THAT(
      SSMErrorConverter::ConvertSSMError(SSMErrors::THROTTLING, "error"),
      ResultIs(FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED)));
}

TEST(SSMErrorConverter, SucceededToConvertNonHandledSSMErrors) {
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(
                  SSMErrors::MALFORMED_QUERY_STRING, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(
                  SSMErrors::UNSUPPORTED_INVENTORY_SCHEMA_VERSION, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(SSMErrorConverter::ConvertSSMError(
                  SSMErrors::AUTOMATION_DEFINITION_VERSION_NOT_FOUND, "error"),
              ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
}
}  // namespace google::scp::cpio::client_providers::test
