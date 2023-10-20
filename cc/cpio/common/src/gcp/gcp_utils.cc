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

#include "gcp_utils.h"

#include "core/common/global_logger/src/global_logger.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_GCP_ABORTED;
using google::scp::core::errors::SC_GCP_ALREADY_EXISTS;
using google::scp::core::errors::SC_GCP_CANCELLED;
using google::scp::core::errors::SC_GCP_DATA_LOSS;
using google::scp::core::errors::SC_GCP_DEADLINE_EXCEEDED;
using google::scp::core::errors::SC_GCP_FAILED_PRECONDITION;
using google::scp::core::errors::SC_GCP_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::errors::SC_GCP_NOT_FOUND;
using google::scp::core::errors::SC_GCP_OUT_OF_RANGE;
using google::scp::core::errors::SC_GCP_PERMISSION_DENIED;
using google::scp::core::errors::SC_GCP_RESOURCE_EXHAUSTED;
using google::scp::core::errors::SC_GCP_UNAUTHENTICATED;
using google::scp::core::errors::SC_GCP_UNAVAILABLE;
using google::scp::core::errors::SC_GCP_UNIMPLEMENTED;
using google::scp::core::errors::SC_GCP_UNKNOWN;

// Filename for logging errors
static constexpr char kGcpErrorConverter[] = "GcpErrorConverter";

namespace google::scp::cpio::common {

ExecutionResult GcpUtils::GcpErrorConverter(cloud::Status status) noexcept {
  auto status_code = status.code();
  ExecutionResult failure = FailureExecutionResult(SC_GCP_UNKNOWN);
  switch (status_code) {
    case cloud::StatusCode::kOk:
      failure = SuccessExecutionResult();
      break;
    case cloud::StatusCode::kNotFound:
      failure = FailureExecutionResult(SC_GCP_NOT_FOUND);
      break;
    case cloud::StatusCode::kInvalidArgument:
      failure = FailureExecutionResult(SC_GCP_INVALID_ARGUMENT);
      break;
    case cloud::StatusCode::kDeadlineExceeded:
      failure = FailureExecutionResult(SC_GCP_DEADLINE_EXCEEDED);
      break;
    case cloud::StatusCode::kAlreadyExists:
      failure = FailureExecutionResult(SC_GCP_ALREADY_EXISTS);
      break;
    case cloud::StatusCode::kUnimplemented:
      failure = FailureExecutionResult(SC_GCP_UNIMPLEMENTED);
      break;
    case cloud::StatusCode::kOutOfRange:
      failure = FailureExecutionResult(SC_GCP_OUT_OF_RANGE);
      break;
    case cloud::StatusCode::kCancelled:
      failure = FailureExecutionResult(SC_GCP_CANCELLED);
      break;
    case cloud::StatusCode::kAborted:
      failure = FailureExecutionResult(SC_GCP_ABORTED);
      break;
    case cloud::StatusCode::kUnavailable:
      failure = FailureExecutionResult(SC_GCP_UNAVAILABLE);
      break;
    case cloud::StatusCode::kUnauthenticated:
      failure = FailureExecutionResult(SC_GCP_UNAUTHENTICATED);
      break;
    case cloud::StatusCode::kPermissionDenied:
      failure = FailureExecutionResult(SC_GCP_PERMISSION_DENIED);
      break;
    case cloud::StatusCode::kDataLoss:
      failure = FailureExecutionResult(SC_GCP_DATA_LOSS);
      break;
    case cloud::StatusCode::kFailedPrecondition:
      failure = FailureExecutionResult(SC_GCP_FAILED_PRECONDITION);
      break;
    case cloud::StatusCode::kResourceExhausted:
      failure = FailureExecutionResult(SC_GCP_RESOURCE_EXHAUSTED);
      break;
    case cloud::StatusCode::kInternal:
      failure = FailureExecutionResult(SC_GCP_INTERNAL_SERVICE_ERROR);
      break;
    default:
      failure = FailureExecutionResult(SC_GCP_UNKNOWN);
  }

  if (!failure.Successful()) {
    SCP_ERROR(kGcpErrorConverter, kZeroUuid, failure,
              "GCP cloud service error: code is %d, and error message is %s.",
              status_code, status.message().c_str());
  }

  return failure;
}

ExecutionResult GcpUtils::GcpErrorConverter(grpc::Status status) noexcept {
  auto status_code = status.error_code();
  ExecutionResult failure = FailureExecutionResult(SC_GCP_UNKNOWN);
  switch (status_code) {
    case grpc::StatusCode::OK:
      failure = SuccessExecutionResult();
      break;
    case grpc::StatusCode::NOT_FOUND:
      failure = FailureExecutionResult(SC_GCP_NOT_FOUND);
      break;
    case grpc::StatusCode::INVALID_ARGUMENT:
      failure = FailureExecutionResult(SC_GCP_INVALID_ARGUMENT);
      break;
    case grpc::StatusCode::DEADLINE_EXCEEDED:
      failure = FailureExecutionResult(SC_GCP_DEADLINE_EXCEEDED);
      break;
    case grpc::StatusCode::ALREADY_EXISTS:
      failure = FailureExecutionResult(SC_GCP_ALREADY_EXISTS);
      break;
    case grpc::StatusCode::UNIMPLEMENTED:
      failure = FailureExecutionResult(SC_GCP_UNIMPLEMENTED);
      break;
    case grpc::StatusCode::OUT_OF_RANGE:
      failure = FailureExecutionResult(SC_GCP_OUT_OF_RANGE);
      break;
    case grpc::StatusCode::CANCELLED:
      failure = FailureExecutionResult(SC_GCP_CANCELLED);
      break;
    case grpc::StatusCode::ABORTED:
      failure = FailureExecutionResult(SC_GCP_ABORTED);
      break;
    case grpc::StatusCode::UNAVAILABLE:
      failure = FailureExecutionResult(SC_GCP_UNAVAILABLE);
      break;
    case grpc::StatusCode::UNAUTHENTICATED:
      failure = FailureExecutionResult(SC_GCP_UNAUTHENTICATED);
      break;
    case grpc::StatusCode::PERMISSION_DENIED:
      failure = FailureExecutionResult(SC_GCP_PERMISSION_DENIED);
      break;
    case grpc::StatusCode::DATA_LOSS:
      failure = FailureExecutionResult(SC_GCP_DATA_LOSS);
      break;
    case grpc::StatusCode::FAILED_PRECONDITION:
      failure = FailureExecutionResult(SC_GCP_FAILED_PRECONDITION);
      break;
    case grpc::StatusCode::RESOURCE_EXHAUSTED:
      failure = FailureExecutionResult(SC_GCP_RESOURCE_EXHAUSTED);
      break;
    case grpc::StatusCode::INTERNAL:
      failure = FailureExecutionResult(SC_GCP_INTERNAL_SERVICE_ERROR);
      break;
    default:
      failure = FailureExecutionResult(SC_GCP_UNKNOWN);
  }

  if (!failure.Successful()) {
    SCP_ERROR(kGcpErrorConverter, kZeroUuid, failure,
              "GCP gRPC service error: code is %d, and error message is %s.",
              status_code, status.error_message().c_str());
  }

  return failure;
}

}  // namespace google::scp::cpio::common
