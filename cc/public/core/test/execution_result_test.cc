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

#include "public/core/interface/execution_result.h"

#include <gtest/gtest.h>

#include "core/common/proto/common.pb.h"
#include "public/core/test/interface/execution_result_test_lib.h"

using std::string;
using testing::_;
using testing::Eq;
using testing::Not;
using testing::UnorderedPointwise;

namespace google::scp::core::test {
TEST(ExecutionResultTest, ToProto) {
  auto success = SuccessExecutionResult();
  auto actual_result = success.ToProto();
  EXPECT_EQ(actual_result.status(),
            core::common::proto::ExecutionStatus::EXECUTION_STATUS_SUCCESS);
  EXPECT_EQ(actual_result.status_code(), 0);

  FailureExecutionResult failure(2);
  actual_result = failure.ToProto();
  EXPECT_EQ(actual_result.status(),
            core::common::proto::ExecutionStatus::EXECUTION_STATUS_FAILURE);
  EXPECT_EQ(actual_result.status_code(), 2);

  RetryExecutionResult retry(2);
  actual_result = retry.ToProto();
  EXPECT_EQ(actual_result.status(),
            core::common::proto::ExecutionStatus::EXECUTION_STATUS_RETRY);
  EXPECT_EQ(actual_result.status_code(), 2);
}

TEST(ExecutionResultTest, FromProto) {
  core::common::proto::ExecutionResult success_proto;
  success_proto.set_status(
      core::common::proto::ExecutionStatus::EXECUTION_STATUS_SUCCESS);
  auto actual_result = ExecutionResult(success_proto);
  EXPECT_EQ(actual_result.status, ExecutionStatus::Success);
  EXPECT_EQ(actual_result.status_code, 0);

  core::common::proto::ExecutionResult failure_proto;
  failure_proto.set_status(
      core::common::proto::ExecutionStatus::EXECUTION_STATUS_FAILURE);
  failure_proto.set_status_code(2);
  actual_result = ExecutionResult(failure_proto);
  EXPECT_EQ(actual_result.status, ExecutionStatus::Failure);
  EXPECT_EQ(actual_result.status_code, 2);

  core::common::proto::ExecutionResult retry_proto;
  retry_proto.set_status(
      core::common::proto::ExecutionStatus::EXECUTION_STATUS_RETRY);
  retry_proto.set_status_code(2);
  actual_result = ExecutionResult(retry_proto);
  EXPECT_EQ(actual_result.status, ExecutionStatus::Retry);
  EXPECT_EQ(actual_result.status_code, 2);
}

TEST(ExecutionResultTest, FromUnknownProto) {
  core::common::proto::ExecutionResult unknown_proto;
  unknown_proto.set_status(
      core::common::proto::ExecutionStatus::EXECUTION_STATUS_UNKNOWN);
  auto actual_result = ExecutionResult(unknown_proto);
  EXPECT_EQ(actual_result.status, ExecutionStatus::Failure);
  EXPECT_EQ(actual_result.status_code, 0);
}

TEST(ExecutionResultTest, MatcherTest) {
  ExecutionResult result1(ExecutionStatus::Failure, 1);
  EXPECT_THAT(result1, ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(result1, Not(IsSuccessful()));

  ExecutionResultOr<int> result_or(result1);
  EXPECT_THAT(result1, ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(result1, Not(IsSuccessful()));

  std::vector<ExecutionResult> results;
  results.push_back(ExecutionResult(ExecutionStatus::Failure, 1));
  results.push_back(ExecutionResult(ExecutionStatus::Retry, 2));

  std::vector<ExecutionResult> expected_results;
  expected_results.push_back(ExecutionResult(ExecutionStatus::Retry, 2));
  expected_results.push_back(ExecutionResult(ExecutionStatus::Failure, 1));
  EXPECT_THAT(results, UnorderedPointwise(ResultIs(), expected_results));
}

TEST(ExecutionResultOrTest, Constructor) {
  // Default.
  ExecutionResultOr<int> result_or1;
  EXPECT_THAT(result_or1.result(), ResultIs(ExecutionResult()));
  EXPECT_FALSE(result_or1.has_value());

  // From Value.
  ExecutionResultOr<int> result_or2(1);
  EXPECT_THAT(result_or2, IsSuccessfulAndHolds(Eq(1)));

  // From Result.
  ExecutionResult result(ExecutionStatus::Failure, 1);
  ExecutionResultOr<int> result_or3(result);
  EXPECT_THAT(result_or3, ResultIs(result));
}

TEST(ExecutionResultOrTest, ExecutionResultMethods) {
  ExecutionResultOr<int> subject(1);
  EXPECT_TRUE(subject.Successful());
  EXPECT_THAT(subject.result(), IsSuccessful());

  subject = ExecutionResult(ExecutionStatus::Failure, 2);
  EXPECT_FALSE(subject.Successful());
  EXPECT_THAT(subject.result(), Not(IsSuccessful()));
}

TEST(ExecutionResultOrTest, ValueMethods) {
  ExecutionResultOr<int> subject(1);
  EXPECT_TRUE(subject.has_value());

  EXPECT_EQ(subject.value(), 1);

  EXPECT_EQ(*subject, 1);

  subject.value() = 2;
  EXPECT_EQ(subject.value(), 2);

  *subject = 3;
  EXPECT_EQ(subject.value(), 3);

  ExecutionResultOr<std::string> subject_2("start");
  subject_2->clear();
  EXPECT_THAT(subject_2, IsSuccessfulAndHolds(Eq("")));

  // Applies const to subject_2.
  const auto& subject_3 = subject_2;
  EXPECT_TRUE(subject_3->empty());
}

TEST(ExecutionResultOrTest, DeathTests) {
  EXPECT_ANY_THROW({
    ExecutionResultOr<std::string> subject(
        ExecutionResult(ExecutionStatus::Failure, 2));
    subject.value();
  });
  EXPECT_ANY_THROW({
    ExecutionResultOr<std::string> subject(
        ExecutionResult(ExecutionStatus::Failure, 2));
    *subject;
  });
  EXPECT_DEATH(
      {
        ExecutionResultOr<std::string> subject(
            ExecutionResult(ExecutionStatus::Failure, 2));
        bool e = subject->empty();
        subject = std::string(e ? "t" : "f");
      },
      _);
}

ExecutionResultOr<std::string> StringOrResult(bool return_string) {
  if (return_string)
    return "returning a string";
  else
    return ExecutionResult(ExecutionStatus::Failure, 1);
}

TEST(ExecutionResultOrTest, FunctionalTest) {
  EXPECT_THAT(StringOrResult(true),
              IsSuccessfulAndHolds(Eq("returning a string")));
  EXPECT_THAT(StringOrResult(false),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
}

}  // namespace google::scp::core::test
