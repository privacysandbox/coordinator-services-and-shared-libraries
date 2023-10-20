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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/proto/common.pb.h"
#include "core/interface/async_context.h"
#include "core/logger/mock/mock_logger.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::common::GlobalLogger;
using google::scp::core::logger::mock::MockLogger;
using std::function;
using std::make_unique;
using std::move;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::FieldsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;
using testing::Pointee;
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

TEST(MacroTest, RETURN_IF_FAILURETest) {
  {
    auto helper = [](ExecutionResult result,
                     bool& succeeded) -> ExecutionResult {
      RETURN_IF_FAILURE(result);
      succeeded = true;
      return SuccessExecutionResult();
    };

    bool succeeded = false;
    // Basic returns error.
    EXPECT_THAT(helper(ExecutionResult(ExecutionStatus::Failure, 1), succeeded),
                ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
    EXPECT_FALSE(succeeded);
    // Basic returns success.
    succeeded = false;
    EXPECT_THAT(helper(SuccessExecutionResult(), succeeded), IsSuccessful());
    EXPECT_TRUE(succeeded);
  }

  {
    auto helper = [](function<ExecutionResult()> fun,
                     bool& succeeded) -> ExecutionResult {
      RETURN_IF_FAILURE(fun());
      succeeded = true;
      return SuccessExecutionResult();
    };
    // Returns error.
    bool succeeded = false;
    EXPECT_THAT(
        helper([]() { return ExecutionResult(ExecutionStatus::Failure, 1); },
               succeeded),
        ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
    EXPECT_FALSE(succeeded);

    // Returns success.
    succeeded = false;
    EXPECT_THAT(helper([]() { return SuccessExecutionResult(); }, succeeded),
                IsSuccessful());
    EXPECT_TRUE(succeeded);

    // Calls exactly once on failure.
    succeeded = false;
    int call_count = 0;
    EXPECT_THAT(helper(
                    [&call_count]() {
                      call_count++;
                      return ExecutionResult(ExecutionStatus::Failure, 1);
                    },
                    succeeded),
                ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
    EXPECT_FALSE(succeeded);
    EXPECT_EQ(call_count, 1);

    // Calls exactly once on success.
    succeeded = false;
    call_count = 0;
    EXPECT_THAT(helper(
                    [&call_count]() {
                      call_count++;
                      return SuccessExecutionResult();
                    },
                    succeeded),
                IsSuccessful());
    EXPECT_TRUE(succeeded);
    EXPECT_EQ(call_count, 1);
  }
}

class MacroLogTest : public testing::Test {
 protected:
  MacroLogTest() {
    auto mock_logger = make_unique<MockLogger>();
    logger_ = mock_logger.get();
    unique_ptr<LoggerInterface> logger = move(mock_logger);
    logger->Init();
    logger->Run();
    GlobalLogger::SetGlobalLogger(std::move(logger));
  }

  ~MacroLogTest() { GlobalLogger::GetGlobalLogger()->Stop(); }

  MockLogger* logger_;
};

TEST_F(MacroLogTest, RETURN_IF_FAILURELogTest) {
  auto helper1 = [](ExecutionResult result) -> ExecutionResult {
    string some_str = "s";
    AsyncContext<int, int> ctx;
    RETURN_AND_LOG_IF_FAILURE_CONTEXT(result, "component", ctx, "msg %s",
                                      some_str.c_str());
    return SuccessExecutionResult();
  };
  // Doesn't log with context.
  EXPECT_THAT(helper1(SuccessExecutionResult()), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), IsEmpty());
  // Logs with context.
  EXPECT_THAT(helper1(FailureExecutionResult(SC_UNKNOWN)),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));

  auto helper2 = [](ExecutionResult result) -> ExecutionResult {
    string some_str = "s";
    RETURN_AND_LOG_IF_FAILURE(result, "component", common::kZeroUuid, "msg %s",
                              some_str.c_str());
    return SuccessExecutionResult();
  };
  // Doesn't log without context.
  EXPECT_THAT(helper2(SuccessExecutionResult()), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));
  // Logs without context.
  EXPECT_THAT(helper2(FailureExecutionResult(SC_UNKNOWN)),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(logger_->GetMessages(),
              ElementsAre(HasSubstr("msg s"), HasSubstr("msg s")));
}

TEST(MacroTest, ASSIGN_OR_RETURNBasicTest) {
  auto helper = [](ExecutionResultOr<int> result_or,
                   int& val) -> ExecutionResult {
    ASSIGN_OR_RETURN(val, result_or);
    // Call ASSIGN_OR_RETURN again in the same scope to test that temp variables
    // have unique names.
    ASSIGN_OR_RETURN(val, ExecutionResultOr<int>(val));
    val++;
    return SuccessExecutionResult();
  };

  int val;
  ExecutionResultOr<int> result_or(5);
  EXPECT_THAT(helper(result_or, val), IsSuccessful());
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  EXPECT_THAT(helper(result_or, val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_EQ(val, 0);
}

TEST_F(MacroLogTest, ASSIGN_OR_RETURNLogTest) {
  auto helper1 = [](ExecutionResultOr<int> result_or,
                    int& val) -> ExecutionResult {
    AsyncContext<int, int> ctx;
    ASSIGN_OR_LOG_AND_RETURN_CONTEXT(val, result_or, "component", ctx, "msg %d",
                                     val);
    val++;
    return SuccessExecutionResult();
  };

  int val;
  ExecutionResultOr<int> result_or(5);
  EXPECT_THAT(helper1(result_or, val), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), IsEmpty());
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  EXPECT_THAT(helper1(result_or, val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg 0")));
  EXPECT_EQ(val, 0);

  auto helper2 = [](ExecutionResultOr<int> result_or,
                    int& val) -> ExecutionResult {
    ASSIGN_OR_LOG_AND_RETURN(val, result_or, "component", common::kZeroUuid,
                             "msg %d", val);
    val++;
    return SuccessExecutionResult();
  };

  result_or = 5;
  EXPECT_THAT(helper2(result_or, val), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg 0")));
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  EXPECT_THAT(helper2(result_or, val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(logger_->GetMessages(),
              ElementsAre(HasSubstr("msg 0"), HasSubstr("msg 0")));
  EXPECT_EQ(val, 0);
}

TEST(MacroTest, ASSIGN_OR_RETURNFunctionTest) {
  auto helper = [](function<ExecutionResultOr<int>()> fun,
                   int& val) -> ExecutionResult {
    ASSIGN_OR_RETURN(val, fun());
    val++;
    return SuccessExecutionResult();
  };
  int val;
  EXPECT_THAT(helper([]() -> ExecutionResultOr<int> { return 5; }, val),
              IsSuccessful());
  EXPECT_EQ(val, 6);

  val = 0;
  EXPECT_THAT(helper(
                  []() -> ExecutionResultOr<int> {
                    return ExecutionResult(ExecutionStatus::Failure, 1);
                  },
                  val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_EQ(val, 0);

  // Success executes once.
  val = 0;
  int call_count = 0;
  EXPECT_THAT(helper(
                  [&call_count]() -> ExecutionResultOr<int> {
                    call_count++;
                    return 5;
                  },
                  val),
              IsSuccessful());
  EXPECT_EQ(val, 6);
  EXPECT_EQ(call_count, 1);

  // Failure executes once.
  val = 0;
  call_count = 0;
  EXPECT_THAT(helper(
                  [&call_count]() -> ExecutionResultOr<int> {
                    call_count++;
                    return ExecutionResult(ExecutionStatus::Failure, 1);
                  },
                  val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_EQ(val, 0);
  EXPECT_EQ(call_count, 1);
}

TEST(MacroTest, ASSIGN_OR_RETURNDeclareWorksInline) {
  auto helper = [](ExecutionResultOr<int> result_or) -> ExecutionResultOr<int> {
    ASSIGN_OR_RETURN(auto ret, result_or);
    return ret;
  };
  EXPECT_THAT(helper(5), IsSuccessfulAndHolds(Eq(5)));
}

TEST(MacroTest, ASSIGN_OR_RETURNWorksWithInnerMembers) {
  auto helper = [](ExecutionResultOr<int> result_or) -> ExecutionResultOr<int> {
    pair<int, string> pair;
    ASSIGN_OR_RETURN(pair.first, result_or);
    return pair.first;
  };
  EXPECT_THAT(helper(5), IsSuccessfulAndHolds(Eq(5)));
}

class NoCopyNoDefault {
 public:
  NoCopyNoDefault() = delete;
  NoCopyNoDefault(const NoCopyNoDefault&) = delete;
  NoCopyNoDefault& operator=(const NoCopyNoDefault&) = delete;

  NoCopyNoDefault(NoCopyNoDefault&&) = default;
  NoCopyNoDefault& operator=(NoCopyNoDefault&&) = default;

  explicit NoCopyNoDefault(unique_ptr<int> x) : x_(move(x)) {}

  unique_ptr<int> x_;
};

TEST(MacroTest, ASSIGN_OR_RETURNWorksWithTemporaryNonCopyableTypes) {
  auto helper1 = [](ExecutionResultOr<NoCopyNoDefault> result_or)
      -> ExecutionResultOr<NoCopyNoDefault> {
    auto foo = [&result_or]() { return move(result_or); };
    ASSIGN_OR_RETURN(auto ret, foo());
    return ret;
  };
  EXPECT_THAT(helper1(NoCopyNoDefault(make_unique<int>(5))),
              IsSuccessfulAndHolds(FieldsAre(Pointee(Eq(5)))));

  auto helper2 = [](ExecutionResultOr<NoCopyNoDefault> result_or)
      -> ExecutionResultOr<NoCopyNoDefault> {
    ASSIGN_OR_RETURN(auto ret, move(result_or));
    return ret;
  };
  EXPECT_THAT(helper2(NoCopyNoDefault(make_unique<int>(5))),
              IsSuccessfulAndHolds(FieldsAre(Pointee(Eq(5)))));
}

TEST(ExecutionResultTest, MatcherTest) {
  ExecutionResult result1(ExecutionStatus::Failure, 1);
  EXPECT_THAT(result1, ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(result1, Not(IsSuccessful()));

  auto result1_proto = result1.ToProto();
  EXPECT_THAT(result1_proto,
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(result1_proto, Not(IsSuccessful()));

  ExecutionResultOr<int> result_or(result1);
  EXPECT_THAT(result1, ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(result1, Not(IsSuccessful()));

  vector<ExecutionResult> results;
  results.push_back(ExecutionResult(ExecutionStatus::Failure, 1));
  results.push_back(ExecutionResult(ExecutionStatus::Retry, 2));

  vector<ExecutionResult> expected_results;
  expected_results.push_back(ExecutionResult(ExecutionStatus::Retry, 2));
  expected_results.push_back(ExecutionResult(ExecutionStatus::Failure, 1));
  EXPECT_THAT(results, UnorderedPointwise(ResultIs(), expected_results));

  EXPECT_SUCCESS(SuccessExecutionResult());
  ASSERT_SUCCESS(SuccessExecutionResult());
  ExecutionResult result = SuccessExecutionResult();
  EXPECT_SUCCESS(result);
  ASSERT_SUCCESS(result);
  result_or = 1;
  EXPECT_SUCCESS(result_or);
  ASSERT_SUCCESS(result_or);
  ASSERT_SUCCESS_AND_ASSIGN(auto value, result_or);
  EXPECT_EQ(value, 1);
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

  ExecutionResultOr<string> subject_2("start");
  subject_2->clear();
  EXPECT_THAT(subject_2, IsSuccessfulAndHolds(Eq("")));

  // Applies const to subject_2.
  const auto& subject_3 = subject_2;
  EXPECT_TRUE(subject_3->empty());
}

TEST(ExecutionResultOrTest, DeathTests) {
  EXPECT_ANY_THROW({
    ExecutionResultOr<string> subject(
        ExecutionResult(ExecutionStatus::Failure, 2));
    subject.value();
  });
  EXPECT_ANY_THROW({
    ExecutionResultOr<string> subject(
        ExecutionResult(ExecutionStatus::Failure, 2));
    *subject;
  });
  EXPECT_DEATH(
      {
        ExecutionResultOr<string> subject(
            ExecutionResult(ExecutionStatus::Failure, 2));
        bool e = subject->empty();
        subject = string(e ? "t" : "f");
      },
      _);
}

TEST(ExecutionResultOrTest, FunctionalTest) {
  auto string_or_result = [](bool return_string) -> ExecutionResultOr<string> {
    if (return_string)
      return "returning a string";
    else
      return ExecutionResult(ExecutionStatus::Failure, 1);
  };

  EXPECT_THAT(string_or_result(true),
              IsSuccessfulAndHolds(Eq("returning a string")));
  EXPECT_THAT(string_or_result(false),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
}

TEST(ExecutionResultOrTest, MoveTest_operator_star) {
  NoCopyNoDefault ncnd(make_unique<int>(5));
  // ExecutionResultOr<NoCopyNoDefault> result_or(ncnd);  // Won't compile.
  ExecutionResultOr<NoCopyNoDefault> result_or(move(ncnd));

  // NoCopyNoDefault other = *result_or;  // Won't compile.

  NoCopyNoDefault other = *move(result_or);
  EXPECT_EQ(ncnd.x_, nullptr);
  // result_or contains the argument of a move constructor after moving.
  ASSERT_TRUE(result_or.has_value());
  EXPECT_EQ(result_or.value().x_, nullptr);
  EXPECT_THAT(other.x_, Pointee(Eq(5)));
}

TEST(ExecutionResultOrTest, MoveTest_value) {
  NoCopyNoDefault ncnd(make_unique<int>(5));
  ExecutionResultOr<NoCopyNoDefault> result_or(move(ncnd));

  NoCopyNoDefault other = move(result_or).value();
  EXPECT_EQ(ncnd.x_, nullptr);
  // result_or contains the argument of a move constructor after moving.
  ASSERT_TRUE(result_or.has_value());
  EXPECT_EQ(result_or.value().x_, nullptr);
  EXPECT_THAT(other.x_, Pointee(Eq(5)));
}

TEST(ExecutionResultOrTest, MoveTest_release) {
  NoCopyNoDefault ncnd(make_unique<int>(5));
  ExecutionResultOr<NoCopyNoDefault> result_or(move(ncnd));

  // No need of writing move!
  NoCopyNoDefault other = result_or.release();
  EXPECT_EQ(ncnd.x_, nullptr);
  // result_or contains the argument of a move constructor after moving.
  ASSERT_TRUE(result_or.has_value());
  EXPECT_EQ(result_or.value().x_, nullptr);
  EXPECT_THAT(other.x_, Pointee(Eq(5)));
}

TEST(ExecutionResultOrTest, DiscardedMoveResult) {
  NoCopyNoDefault ncnd(make_unique<int>(5));
  ExecutionResultOr<NoCopyNoDefault> result_or(move(ncnd));

  // We expect that just calling operator* && does not invalidate the object.
  *move(result_or);
  ASSERT_TRUE(result_or.has_value());
  ASSERT_THAT(result_or->x_, Pointee(Eq(5)));

  // We expect that just calling value() && does not invalidate the object.
  move(result_or).value();
  ASSERT_TRUE(result_or.has_value());
  ASSERT_THAT(result_or->x_, Pointee(Eq(5)));
}

}  // namespace google::scp::core::test
