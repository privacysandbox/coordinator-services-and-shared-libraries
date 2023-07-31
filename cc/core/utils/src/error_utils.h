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

#include "public/core/interface/execution_result.h"

namespace google::scp::core::utils {
/**
 * @brief Convert internal execution result to public result
 *
 * @param executions_result the given internal execution result.
 * @return ExecutionResult returned public execution result.
 */
ExecutionResult ConvertToPublicExecutionResult(
    const ExecutionResult& execution_result);
}  // namespace google::scp::core::utils
