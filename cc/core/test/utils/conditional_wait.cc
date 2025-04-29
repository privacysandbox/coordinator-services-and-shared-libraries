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

#include "conditional_wait.h"

#include <chrono>
#include <functional>
#include <thread>

#include "absl/strings/str_cat.h"
#include "cc/core/common/time_provider/src/time_provider.h"
#include "cc/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace privacy_sandbox::pbs_common {
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::test::DurationMs;
using ::privacy_sandbox::pbs_common::SC_TEST_UTILS_TEST_WAIT_TIMEOUT;
using ::privacy_sandbox::pbs_common::TestTimeoutException;
using ::privacy_sandbox::pbs_common::TimeProvider;
using std::function;
using std::this_thread::yield;

void WaitUntil(function<bool()> condition, DurationMs timeout) {
  auto start_time = TimeProvider::GetSteadyTimestampInNanoseconds();
  while (!condition()) {
    auto now_time = TimeProvider::GetSteadyTimestampInNanoseconds();
    auto duration = now_time - start_time;
    if (duration > timeout) {
      std::cerr << absl::StrCat(
          "WaitUntil throwing TestTimeoutException: Waited for (ms): ",
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::nanoseconds(duration))
              .count());
      throw TestTimeoutException();
    }
    yield();
  }
}

ExecutionResult WaitUntilOrReturn(function<bool()> condition,
                                  DurationMs timeout) noexcept {
  try {
    WaitUntil(condition, timeout);
  } catch (const TestTimeoutException&) {
    return FailureExecutionResult(SC_TEST_UTILS_TEST_WAIT_TIMEOUT);
  }
  return SuccessExecutionResult();
}

}  // namespace privacy_sandbox::pbs_common
