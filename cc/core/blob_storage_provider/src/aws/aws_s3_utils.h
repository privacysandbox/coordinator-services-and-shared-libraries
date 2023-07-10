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

#include "core/blob_storage_provider/src/common/error_codes.h"
#include "core/interface/blob_storage_provider_interface.h"

namespace google::scp::core::blob_storage_provider {
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
      case Aws::S3::S3Errors::SERVICE_UNAVAILABLE:
      case Aws::S3::S3Errors::THROTTLING:
        return core::RetryExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR);

      case Aws::S3::S3Errors::NO_SUCH_KEY:
        return core::FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND);
      default:
        return core::FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR);
    }
  }
};
}  // namespace google::scp::core::blob_storage_provider
