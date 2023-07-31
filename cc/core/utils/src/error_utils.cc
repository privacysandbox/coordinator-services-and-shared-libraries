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

#include "error_utils.h"

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::errors::GetPublicErrorCode;

namespace google::scp::core::utils {
ExecutionResult ConvertToPublicExecutionResult(
    const ExecutionResult& execution_result) {
  if (execution_result.Successful()) {
    return execution_result;
  }
  auto public_failure_result = execution_result;
  public_failure_result.status_code =
      GetPublicErrorCode(execution_result.status_code);
  return public_failure_result;
}
}  // namespace google::scp::core::utils
