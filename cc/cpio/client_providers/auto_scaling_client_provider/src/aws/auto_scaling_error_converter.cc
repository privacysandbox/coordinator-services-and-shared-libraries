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

#include "auto_scaling_error_converter.h"

#include <string>

#include <aws/autoscaling/AutoScalingClient.h>
#include <aws/core/Aws.h>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "cpio/common/src/aws/error_codes.h"

#include "error_codes.h"

using Aws::AutoScaling::AutoScalingErrors;
using Aws::Client::AWSError;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;

// Filename for logging errors
static constexpr char kAutoScalingErrorConverter[] =
    "AutoScalingErrorConverter";

namespace google::scp::cpio::client_providers {
ExecutionResult AutoScalingErrorConverter::ConvertAutoScalingError(
    const AWSError<AutoScalingErrors>& error) {
  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  switch (error.GetErrorType()) {
    case AutoScalingErrors::VALIDATION:
      failure = FailureExecutionResult(SC_AWS_VALIDATION_FAILED);
      break;
    case AutoScalingErrors::ACCESS_DENIED:
    case AutoScalingErrors::INVALID_CLIENT_TOKEN_ID:
      failure = FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS);
      break;
    case AutoScalingErrors::INVALID_PARAMETER_COMBINATION:
    case AutoScalingErrors::INVALID_QUERY_PARAMETER:
    case AutoScalingErrors::INVALID_PARAMETER_VALUE:
    case AutoScalingErrors::MALFORMED_QUERY_STRING:
      failure = FailureExecutionResult(SC_AWS_INVALID_REQUEST);
      break;
    case AutoScalingErrors::SERVICE_UNAVAILABLE:
    case AutoScalingErrors::NETWORK_CONNECTION:
      failure = FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE);
      break;
    case AutoScalingErrors::THROTTLING:
      failure = FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED);
      break;
    case AutoScalingErrors::INTERNAL_FAILURE:
    default:
      failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  }

  SCP_ERROR(kAutoScalingErrorConverter, kZeroUuid, failure,
            "AWS cloud service error: code is %d, and error message is %s.",
            error.GetErrorType(), error.GetMessage().c_str());
  return failure;
}
}  // namespace google::scp::cpio::client_providers
