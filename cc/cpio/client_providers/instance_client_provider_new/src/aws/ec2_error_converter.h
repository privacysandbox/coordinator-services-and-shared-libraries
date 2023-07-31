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

#include <string>

#include <aws/ec2/EC2Client.h>

#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/// Converts EC2Errors to our errors.
class EC2ErrorConverter {
 public:
  /**
   * @brief Converts EC2 Error to ExecutionResult with failure status
   *
   * @param error The EC2 error comes from AWS.
   *
   * @return core::FailureExecutionResult The converted result of the operation.
   */
  static core::FailureExecutionResult ConvertEC2Error(
      const Aws::EC2::EC2Errors& error, const std::string& error_message);
};
}  // namespace google::scp::cpio::client_providers
