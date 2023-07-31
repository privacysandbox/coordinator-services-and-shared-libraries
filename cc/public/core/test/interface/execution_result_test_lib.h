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

#include <gtest/gtest.h>

#include <string>
#include <type_traits>

#include <gmock/gmock-matchers.h>

#include "absl/strings/str_format.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::test {
namespace internal {
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
}  // namespace internal

// Matches arg with expected_result.
// Example usage:
// ExecutionResult result = foo();
// ExecutionResult expected_result = ...;
// EXPECT_THAT(result, ResultIs(expected_result));
//
// Note that this also works if arg is an ExecutionResultOr - the expected
// argument should still be an ExecutionResult:
// ExecutionResultOr<int> result_or = foo();
// ExecutionResult expected_result = ...;
// EXPECT_THAT(result, ResultIs(expected_result));
MATCHER_P(ResultIs, expected_result, "") {
  auto execution_result_to_str = [](ExecutionResult result) {
    return absl::StrFormat(
        "ExecutionStatus: %s\n\t"
        "StatusCode: %d\n\t"
        "ErrorMessage: \"%s\"\n",
        internal::ToString(result.status), result.status_code,
        GetErrorMessage(result.status_code));
  };
  if constexpr (std::is_same_v<
                    google::scp::core::ExecutionResult,
                    std::remove_cv_t<std::remove_reference_t<decltype(arg)>>>) {
    // If arg is an ExecutionResult - directly compare.
    if (arg != expected_result) {
      *result_listener << absl::StrFormat(
          "\nExpected result to have:\n\t%s"
          "Actual result has:\n\t%s",
          execution_result_to_str(expected_result),
          execution_result_to_str(arg));
      return false;
    }
  } else {
    // arg is an ExecutionResultOr - call ::result() and compare.
    if (arg.result() != expected_result) {
      *result_listener << absl::StrFormat(
          "\nExpected result to have:\n\t%s"
          "Actual result has:\n\t%s",
          execution_result_to_str(expected_result),
          execution_result_to_str(arg.result()));
      return false;
    }
  }
  return true;
}

// Overload for the above but for no arguments - useful for Pointwise usage.
// Example:
// std::vector<ExecutionResult> results = foo();
// std::vector<ExecutionResult> expected_results = ...;
// // Checks that results contains some permutation of expected_results.
// EXPECT_THAT(results, UnorderedPointwise(ResultIs(), expected_results));
MATCHER(ResultIs, "") {
  const auto& [actual, expected] = arg;
  return ::testing::ExplainMatchResult(ResultIs(expected), actual,
                                       result_listener);
}

// Expects that arg.Successful() is true - i.e. arg == SuccessExecutionResult().
MATCHER(IsSuccessful, "") {
  return ::testing::ExplainMatchResult(ResultIs(SuccessExecutionResult()), arg,
                                       result_listener);
}

// Expects that arg.result().IsSuccessful() is true, and that the value
// held by arg matches inner matcher.
// Example:
// ExecutionResultOr<int> foo();
// EXPECT_THAT(foo(), IsSuccessfulAndHolds(Eq(5)));
MATCHER_P(IsSuccessfulAndHolds, inner_matcher, "") {
  return ::testing::ExplainMatchResult(IsSuccessful(), arg.result(),
                                       result_listener) &&
         ::testing::ExplainMatchResult(inner_matcher, arg.value(),
                                       result_listener);
}

}  // namespace google::scp::core::test
