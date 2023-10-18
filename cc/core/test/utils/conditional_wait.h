// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <chrono>
#include <functional>

#include "core/test/test_config.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::test {

/**
 * @brief Timeout exception object thrown by WaitUntil
 *
 */
class TestTimeoutException : public std::exception {
  const char* what() const noexcept override {
    static constexpr char exception_name[] = "TestTimeoutException";
    return exception_name;
  }
};

/**
 * @brief Waits util the given condition is met.
 *
 * @param condition when the condition is met, stop waiting.
 * @param timeout the maximum time before stop waiting.
 */
void WaitUntil(std::function<bool()> condition,
               DurationMs timeout = UNIT_TEST_TIME_OUT_MS);

/**
 * @brief Waits util the given condition is met. This is a no exception version
 * of WaitUntil.
 *
 * @param condition when the condition is met, stop waiting.
 * @param timeout the maximum time before stop waiting.
 */
ExecutionResult WaitUntilOrReturn(
    std::function<bool()> condition,
    DurationMs timeout = UNIT_TEST_TIME_OUT_MS) noexcept;
}  // namespace google::scp::core::test
