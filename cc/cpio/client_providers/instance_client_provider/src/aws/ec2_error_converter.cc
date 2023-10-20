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

#include "ec2_error_converter.h"

#include <string>

#include <aws/ec2/EC2Client.h>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "cpio/common/src/aws/error_codes.h"

#include "error_codes.h"

using Aws::EC2::EC2Errors;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;

/// Filename for logging errors
static constexpr char kEC2ErrorConverter[] = "EC2ErrorConverter";

namespace google::scp::cpio::client_providers {
FailureExecutionResult EC2ErrorConverter::ConvertEC2Error(
    const EC2Errors& error, const std::string& error_message) {
  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  switch (error) {
    case EC2Errors::VALIDATION:
      failure = FailureExecutionResult(SC_AWS_VALIDATION_FAILED);
      break;
    case EC2Errors::ACCESS_DENIED:
      failure = FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS);
      break;
    case EC2Errors::INVALID_PARAMETER_COMBINATION:
    case EC2Errors::INVALID_QUERY_PARAMETER:
    case EC2Errors::INVALID_PARAMETER_VALUE:
      failure = FailureExecutionResult(SC_AWS_INVALID_REQUEST);
      break;
    case EC2Errors::SERVICE_UNAVAILABLE:
    case EC2Errors::NETWORK_CONNECTION:
      failure = FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE);
      break;
    case EC2Errors::THROTTLING:
      failure = FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED);
      break;
    case EC2Errors::INTERNAL_FAILURE:
    default:
      failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  }
  SCP_ERROR(kEC2ErrorConverter, kZeroUuid, failure,
            "AWS cloud service error: code is %d, and error message is %s.",
            error, error_message.c_str());
  return failure;
}
}  // namespace google::scp::cpio::client_providers
