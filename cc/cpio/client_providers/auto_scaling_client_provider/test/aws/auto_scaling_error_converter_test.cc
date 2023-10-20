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

#include "cpio/client_providers/auto_scaling_client_provider/src/aws/auto_scaling_error_converter.h"

#include <gtest/gtest.h>

#include <string>

#include <aws/autoscaling/AutoScalingErrors.h>
#include <aws/core/Aws.h>

#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::AutoScaling::AutoScalingErrors;
using Aws::Client::AWSError;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::ResultIs;
using std::get;
using std::make_tuple;
using std::string;
using std::tuple;
using testing::TestWithParam;
using testing::Values;

namespace google::scp::cpio::client_providers::test {
class AutoScalingErrorConverterTest
    : public TestWithParam<tuple<AutoScalingErrors, FailureExecutionResult>> {
 protected:
  AWSError<AutoScalingErrors> GetAutoScalingErrorsToConvert() {
    return AWSError<AutoScalingErrors>(get<0>(GetParam()), false);
  }

  FailureExecutionResult GetExpectedFailureExecutionResult() {
    return get<1>(GetParam());
  }
};

TEST_P(AutoScalingErrorConverterTest, AutoScalingErrorConverter) {
  auto error_code = GetAutoScalingErrorsToConvert();
  EXPECT_THAT(AutoScalingErrorConverter::ConvertAutoScalingError(error_code),
              ResultIs(GetExpectedFailureExecutionResult()));
}

INSTANTIATE_TEST_SUITE_P(
    AutoScalingErrorConverterTest, AutoScalingErrorConverterTest,
    Values(make_tuple(AutoScalingErrors::VALIDATION,
                      FailureExecutionResult(SC_AWS_VALIDATION_FAILED)),
           make_tuple(AutoScalingErrors::ACCESS_DENIED,
                      FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)),
           make_tuple(AutoScalingErrors::INVALID_CLIENT_TOKEN_ID,
                      FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)),
           make_tuple(AutoScalingErrors::INVALID_PARAMETER_COMBINATION,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(AutoScalingErrors::INVALID_QUERY_PARAMETER,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(AutoScalingErrors::INVALID_PARAMETER_VALUE,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(AutoScalingErrors::MALFORMED_QUERY_STRING,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(AutoScalingErrors::SERVICE_UNAVAILABLE,
                      FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)),
           make_tuple(AutoScalingErrors::NETWORK_CONNECTION,
                      FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)),
           make_tuple(AutoScalingErrors::THROTTLING,
                      FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED)),
           make_tuple(AutoScalingErrors::INTERNAL_FAILURE,
                      FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR))));
}  // namespace google::scp::cpio::client_providers::test
