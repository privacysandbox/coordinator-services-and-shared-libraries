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

#pragma once

#include "core/blob_storage_provider/src/common/error_codes.h"
#include "google/cloud/status.h"

namespace google::scp::core::blob_storage_provider {
/**
 * @brief Provides utility functions for GCP Cloud Storage request flows. GCP
 * uses custom types that need to be converted to SCP types during runtime.
 */
class GcpCloudStorageUtils {
 public:
  /**
   * @brief Converts Cloud Storage errors to ExecutionResult.
   *
   * @param cloud_storage_error_code Cloud Storage error codes.
   * @return core::ExecutionResult The Cloud Storage error code converted to the
   * execution result.
   */
  static core::ExecutionResult ConvertCloudStorageErrorToExecutionResult(
      const google::cloud::StatusCode cloud_storage_error_code) noexcept {
    // TODO: Fix and improve these mappings. See the following sites for
    // additional context and more information:
    // https://cloud.google.com/storage/docs/retry-strategy#client-libraries
    // https://grpc.github.io/grpc/cpp/namespacegrpc.html#aff1730578c90160528f6a8d67ef5c43b
    // https://cloud.google.com/apis/design/errors#error_info
    // https://grpc.io/grpc/cpp/classgrpc_1_1_status.html

    // Note: The codes kDeadlineExceeded, kUnavailable, kInternal, and
    // kResourceExhausted are not automatically retried by GCP. For all other
    // codes GCP will automatically retry if left unconfigured. This can be
    // turned off or adjusted via the client's Options
    switch (cloud_storage_error_code) {
      case google::cloud::StatusCode::kResourceExhausted:
      case google::cloud::StatusCode::kUnavailable:
      case google::cloud::StatusCode::kInternal:
      case google::cloud::StatusCode::kUnknown:
      case google::cloud::StatusCode::kAborted:
      case google::cloud::StatusCode::kFailedPrecondition:
      // TODO: If kAlreadyExists can apply to blobs, then convert to
      // BLOB_PATH_EXISTS
      case google::cloud::StatusCode::kAlreadyExists:
        return core::RetryExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR);

      case google::cloud::StatusCode::kNotFound:
        return core::FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND);

      case google::cloud::StatusCode::kOutOfRange:
        return core::FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB);

      case google::cloud::StatusCode::kDataLoss:
      case google::cloud::StatusCode::kInvalidArgument:
      case google::cloud::StatusCode::kUnimplemented:
      case google::cloud::StatusCode::kCancelled:
      case google::cloud::StatusCode::kPermissionDenied:
      case google::cloud::StatusCode::kUnauthenticated:
      case google::cloud::StatusCode::kDeadlineExceeded:
      default:
        return core::FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR);
    }
  }
};
}  // namespace google::scp::core::blob_storage_provider
