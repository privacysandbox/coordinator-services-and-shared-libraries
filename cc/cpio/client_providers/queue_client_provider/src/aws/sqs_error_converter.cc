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

#include "sqs_error_converter.h"

#include "cpio/common/src/aws/error_codes.h"

#include "error_codes.h"

using Aws::SQS::SQSErrors;
using google::scp::core::FailureExecutionResult;
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

namespace google::scp::cpio::client_providers {
FailureExecutionResult SqsErrorConverter::ConvertSqsError(
    const SQSErrors& error) {
  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  switch (error) {
    case SQSErrors::VALIDATION:
      failure = FailureExecutionResult(SC_AWS_VALIDATION_FAILED);
      break;
    case SQSErrors::ACCESS_DENIED:
    case SQSErrors::INVALID_CLIENT_TOKEN_ID:
      failure = FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS);
      break;
    case SQSErrors::INVALID_PARAMETER_COMBINATION:
    case SQSErrors::INVALID_QUERY_PARAMETER:
    case SQSErrors::INVALID_PARAMETER_VALUE:
    case SQSErrors::MALFORMED_QUERY_STRING:
      failure = FailureExecutionResult(SC_AWS_INVALID_REQUEST);
      break;
    case SQSErrors::SERVICE_UNAVAILABLE:
    case SQSErrors::NETWORK_CONNECTION:
      failure = FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE);
      break;
    case SQSErrors::THROTTLING:
    case SQSErrors::OVER_LIMIT:
      failure = FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED);
      break;
    case SQSErrors::INVALID_MESSAGE_CONTENTS:
      failure =
          FailureExecutionResult(SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
      break;
    case SQSErrors::MESSAGE_NOT_INFLIGHT:
      failure = FailureExecutionResult(
          SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT);
      break;
    case SQSErrors::RECEIPT_HANDLE_IS_INVALID:
    case SQSErrors::INVALID_ID_FORMAT:
      failure = FailureExecutionResult(
          SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO);
      break;
    case SQSErrors::INTERNAL_FAILURE:
    default:
      failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  }
  return failure;
}
}  // namespace google::scp::cpio::client_providers
