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

#include "sts_error_converter.h"

#include <string>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "cpio/common/src/aws/error_codes.h"

#include "error_codes.h"

using Aws::STS::STSErrors;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;

/// Filename for logging errors
static constexpr char kSTSErrorConverter[] = "STSErrorConverter";

namespace google::scp::cpio::client_providers {
FailureExecutionResult STSErrorConverter::ConvertSTSError(
    const STSErrors& error, const std::string& error_message) {
  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  switch (error) {
    case STSErrors::VALIDATION:
      failure = FailureExecutionResult(SC_AWS_VALIDATION_FAILED);
      break;
    case STSErrors::ACCESS_DENIED:
    case STSErrors::MISSING_AUTHENTICATION_TOKEN:
      failure = FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS);
      break;
    case STSErrors::INVALID_PARAMETER_COMBINATION:
    case STSErrors::INVALID_QUERY_PARAMETER:
    case STSErrors::INVALID_PARAMETER_VALUE:
      failure = FailureExecutionResult(SC_AWS_INVALID_REQUEST);
      break;
    case STSErrors::SERVICE_UNAVAILABLE:
    case STSErrors::NETWORK_CONNECTION:
      failure = FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE);
      break;
    case STSErrors::THROTTLING:
      failure = FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED);
      break;
    case STSErrors::INTERNAL_FAILURE:
    default:
      failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  }

  SCP_ERROR(kSTSErrorConverter, kZeroUuid, failure,
            "AWS cloud service error: code is %d, and error message is %s.",
            error, error_message.c_str());
  return failure;
}
}  // namespace google::scp::cpio::client_providers
