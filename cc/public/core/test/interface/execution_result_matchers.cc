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
#include "execution_result_matchers.h"

#include <string>

#include "public/core/interface/execution_result.h"

namespace google::scp::core::test::internal {
std::string ToString(ExecutionStatus status) {
  switch (status) {
    case ExecutionStatus::Success:
      return "Success";
    case ExecutionStatus::Failure:
      return "Failure";
    case ExecutionStatus::Retry:
      return "Retry";
    default:
      return "UNKNOWN EXECUTIONSTATUS";
  }
}
}  // namespace google::scp::core::test::internal
