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

#include <aws/s3/S3Client.h>

#include "cpio/common/src/aws/error_codes.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Provides utility functions for AWS S3 request flows. Aws uses
 * custom types that need to be converted to SCP types during runtime.
 */
class AwsS3Utils {
 public:
  /**
   * @brief Converts S3 errors to ExecutionResult.
   *
   * @param dynamo_db_error S3 error codes.
   * @return core::ExecutionResult The S3 error code converted to the
   * execution result.
   */
  static core::ExecutionResult ConvertS3ErrorToExecutionResult(
      const Aws::S3::S3Errors& dynamo_db_error) noexcept {
    switch (dynamo_db_error) {
      case Aws::S3::S3Errors::INTERNAL_FAILURE:
        return core::FailureExecutionResult(
            core::errors::SC_AWS_INTERNAL_SERVICE_ERROR);
      case Aws::S3::S3Errors::SERVICE_UNAVAILABLE:
      case Aws::S3::S3Errors::THROTTLING:
        return core::RetryExecutionResult(
            core::errors::SC_AWS_SERVICE_UNAVAILABLE);
      case Aws::S3::S3Errors::NO_SUCH_KEY:
        return core::FailureExecutionResult(core::errors::SC_AWS_NOT_FOUND);
      default:
        return core::FailureExecutionResult(
            core::errors::SC_AWS_INTERNAL_SERVICE_ERROR);
    }
  }
};
}  // namespace google::scp::cpio::client_providers
