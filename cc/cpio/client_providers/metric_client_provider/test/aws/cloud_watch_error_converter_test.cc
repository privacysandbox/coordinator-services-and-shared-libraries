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

#include "cpio/client_providers/metric_client_provider/src/aws/cloud_watch_error_converter.h"

#include <gtest/gtest.h>

#include <string>

#include <aws/core/Aws.h>
#include <aws/monitoring/CloudWatchErrors.h>

#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"

using Aws::Client::AWSError;
using Aws::CloudWatch::CloudWatchErrors;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_METRIC_LIMIT_REACHED_PER_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using std::map;

namespace google::scp::cpio::client_providers::test {
TEST(CloudWatchErrorConverterTest, ConvertCloudWatchError) {
  map<CloudWatchErrors, StatusCode> errors_map = {
      {CloudWatchErrors::ACCESS_DENIED, SC_AWS_INVALID_CREDENTIALS},
      {CloudWatchErrors::MISSING_AUTHENTICATION_TOKEN,
       SC_AWS_INVALID_CREDENTIALS},
      {CloudWatchErrors::MISSING_REQUIRED_PARAMETER, SC_AWS_INVALID_REQUEST},
      {CloudWatchErrors::INVALID_PARAMETER_COMBINATION, SC_AWS_INVALID_REQUEST},
      {CloudWatchErrors::INVALID_PARAMETER_VALUE, SC_AWS_INVALID_REQUEST},
      {CloudWatchErrors::SERVICE_UNAVAILABLE, SC_AWS_SERVICE_UNAVAILABLE},
      {CloudWatchErrors::NETWORK_CONNECTION, SC_AWS_SERVICE_UNAVAILABLE},
      {CloudWatchErrors::LIMIT_EXCEEDED,
       SC_AWS_METRIC_CLIENT_PROVIDER_METRIC_LIMIT_REACHED_PER_REQUEST},
      {CloudWatchErrors::THROTTLING, SC_AWS_REQUEST_LIMIT_REACHED},
      {CloudWatchErrors::INTERNAL_FAILURE, SC_AWS_INTERNAL_SERVICE_ERROR},
      {CloudWatchErrors::INVALID_NEXT_TOKEN, SC_AWS_INTERNAL_SERVICE_ERROR}};

  for (const auto& [aws_error, error_status_code] : errors_map) {
    AWSError<CloudWatchErrors> error(aws_error, false);

    auto cpio_error = FailureExecutionResult(error_status_code);
    auto converted_error = CloudWatchErrorConverter::ConvertCloudWatchError(
        error.GetErrorType(), "error");
    EXPECT_EQ(cpio_error, converted_error);
  }
}
}  // namespace google::scp::cpio::client_providers::test
