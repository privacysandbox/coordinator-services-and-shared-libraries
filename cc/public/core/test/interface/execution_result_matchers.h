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

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <string>
#include <type_traits>

#include "absl/strings/str_format.h"
#include "core/common/proto/common.pb.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::test {
namespace internal {
std::string ToString(ExecutionStatus status);
}  // namespace internal

// Macro for shortening this pattern:
// EXPECT_THAT(execution_result, IsSuccessful())
#define EXPECT_SUCCESS(expression) \
  EXPECT_THAT(expression, ::google::scp::core::test::IsSuccessful())

// Macro for shortening this pattern:
// ASSERT_THAT(execution_result, IsSuccessful())
#define ASSERT_SUCCESS(expression) \
  ASSERT_THAT(expression, ::google::scp::core::test::IsSuccessful())

// Macro for shortening this pattern:
// ASSERT_SUCCESS(execution_result_or);
// auto value = execution_result_or.release();
// EXPECT_THAT(value, MatchesSomething());
//
// It becomes:
// ASSERT_SUCCESS_AND_ASSIGN(auto value, execution_result_or);
// EXPECT_THAT(value, MatchesSomething());
#define ASSERT_SUCCESS_AND_ASSIGN(lhs, execution_result_or)            \
  __ASSERT_SUCCESS_AND_ASSIGN_HELPER(lhs, __UNIQUE_VAR_NAME(__LINE__), \
                                     execution_result_or)

#define __ASSERT_SUCCESS_AND_ASSIGN_HELPER(lhs, result_or_temp_var_name, \
                                           execution_result_or)          \
  auto&& result_or_temp_var_name = execution_result_or;                  \
  ASSERT_SUCCESS(result_or_temp_var_name);                               \
  lhs = result_or_temp_var_name.release();

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
  constexpr bool is_proto = std::is_base_of_v<
      google::scp::core::common::proto::ExecutionResult,
      std::remove_cv_t<std::remove_reference_t<decltype(arg)>>>;
  if constexpr (std::is_base_of_v<
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
  } else if constexpr (is_proto) {
    // If arg is an ExecutionResult proto, convert and compare.
    google::scp::core::ExecutionResult non_proto_execution_result(arg);
    if (non_proto_execution_result != expected_result) {
      *result_listener << absl::StrFormat(
          "\nExpected result to have:\n\t%s"
          "Actual result has:\n\t%s"
          "Actual result as a proto:\n%s",
          execution_result_to_str(expected_result),
          execution_result_to_str(non_proto_execution_result),
          arg.DebugString());
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
