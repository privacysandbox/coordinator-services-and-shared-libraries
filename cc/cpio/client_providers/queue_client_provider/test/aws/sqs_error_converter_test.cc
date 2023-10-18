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

#include "cpio/client_providers/queue_client_provider/src/aws/sqs_error_converter.h"

#include <gtest/gtest.h>

#include <string>

#include <aws/core/Aws.h>
#include <aws/sqs/SQSErrors.h>

#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::Client::AWSError;
using Aws::SQS::SQSErrors;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using std::get;
using std::make_tuple;
using std::string;
using std::tuple;
using testing::TestWithParam;
using testing::Values;

namespace google::scp::cpio::client_providers::test {
class SqsErrorConverterTest
    : public TestWithParam<tuple<SQSErrors, FailureExecutionResult>> {
 protected:
  SQSErrors GetSqsErrorsToConvert() { return get<0>(GetParam()); }

  FailureExecutionResult GetExpectedFailureExecutionResult() {
    return get<1>(GetParam());
  }
};

TEST_P(SqsErrorConverterTest, SqsErrorConverter) {
  auto error_code = GetSqsErrorsToConvert();
  EXPECT_EQ(SqsErrorConverter::ConvertSqsError(error_code),
            GetExpectedFailureExecutionResult());
}

INSTANTIATE_TEST_SUITE_P(
    SqsErrorConverterTest, SqsErrorConverterTest,
    Values(make_tuple(SQSErrors::VALIDATION,
                      FailureExecutionResult(SC_AWS_VALIDATION_FAILED)),
           make_tuple(SQSErrors::ACCESS_DENIED,
                      FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)),
           make_tuple(SQSErrors::INVALID_CLIENT_TOKEN_ID,
                      FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)),
           make_tuple(SQSErrors::INVALID_PARAMETER_COMBINATION,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(SQSErrors::INVALID_QUERY_PARAMETER,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(SQSErrors::INVALID_PARAMETER_VALUE,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(SQSErrors::MALFORMED_QUERY_STRING,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(SQSErrors::SERVICE_UNAVAILABLE,
                      FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)),
           make_tuple(SQSErrors::NETWORK_CONNECTION,
                      FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)),
           make_tuple(SQSErrors::THROTTLING,
                      FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED)),
           make_tuple(SQSErrors::OVER_LIMIT,
                      FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED)),
           make_tuple(SQSErrors::INVALID_MESSAGE_CONTENTS,
                      FailureExecutionResult(
                          SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)),
           make_tuple(SQSErrors::MESSAGE_NOT_INFLIGHT,
                      FailureExecutionResult(
                          SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT)),
           make_tuple(SQSErrors::RECEIPT_HANDLE_IS_INVALID,
                      FailureExecutionResult(
                          SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)),
           make_tuple(SQSErrors::INVALID_ID_FORMAT,
                      FailureExecutionResult(
                          SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)),
           make_tuple(SQSErrors::INTERNAL_FAILURE,
                      FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR))));
}  // namespace google::scp::cpio::client_providers::test
