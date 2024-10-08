// Copyright 2024 Google LLC
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
#include "pbs/consume_budget/src/gcp/consume_budget.h"

#include <gtest/gtest.h>

#include <memory>

#include "absl/synchronization/blocking_counter.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/protobuf/text_format.h"

namespace google::scp::pbs {
namespace {

using ::google::protobuf::TextFormat;
using ::google::scp::core::AsyncContext;
using ::google::scp::core::AsyncExecutor;
using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::config_provider::mock::MockConfigProvider;
using ::google::scp::core::errors::SC_ASYNC_EXECUTOR_NOT_RUNNING;
using ::google::scp::core::test::ResultIs;
using ::google::scp::pbs::kBudgetKeyTableName;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_EXHAUSTED;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_INITIALIZATION_ERROR;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_PARSING_ERROR;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
namespace spanner = ::google::cloud::spanner;
namespace spanner_mocks = ::google::cloud::spanner_mocks;

constexpr absl::string_view kBudgetKeySpannerColumnName = "Budget_Key";
constexpr absl::string_view kTimeframeSpannerColumnName = "Timeframe";
constexpr absl::string_view kValueSpannerColumnName = "Value";
constexpr size_t kThreadCount = 5;
constexpr size_t kQueueSize = 100;
constexpr absl::string_view kTableName = "fake-table-name";
constexpr absl::string_view kBudgetKeyTableMetadata = R"pb(
  row_type: {
    fields: {
      name: "Budget_Key",
      type: { code: STRING }
    }
    fields: {
      name: "Timeframe",
      type: { code: STRING }
    }
    fields: {
      name: "Value",
      type: { code: JSON }
    }
  })pb";

std::unique_ptr<spanner_mocks::MockResultSetSource>
CreatePbsMockResultSetSource() {
  auto source =
      std::make_unique<google::cloud::spanner_mocks::MockResultSetSource>();

  google::spanner::v1::ResultSetMetadata metadata;
  EXPECT_TRUE(TextFormat::ParseFromString(std::string(kBudgetKeyTableMetadata),
                                          &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  return source;
}

class BudgetConsumptionHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_connection_ = std::make_shared<spanner_mocks::MockConnection>();
    async_executor_ = std::make_unique<AsyncExecutor>(kThreadCount, kQueueSize);
    io_async_executor_ =
        std::make_unique<AsyncExecutor>(kThreadCount, kQueueSize);
    mock_config_provider_ = std::make_unique<MockConfigProvider>();
    budget_consumption_helper_ = std::make_unique<BudgetConsumptionHelper>(
        mock_config_provider_.get(), async_executor_.get(),
        io_async_executor_.get(), mock_connection_);
  }

  ExecutionResult InitAndRunComponents() {
    RETURN_IF_FAILURE(async_executor_->Init());
    RETURN_IF_FAILURE(io_async_executor_->Init());
    RETURN_IF_FAILURE(mock_config_provider_->Init());
    RETURN_IF_FAILURE(budget_consumption_helper_->Init());
    RETURN_IF_FAILURE(async_executor_->Run());
    RETURN_IF_FAILURE(io_async_executor_->Run());
    RETURN_IF_FAILURE(mock_config_provider_->Run());
    RETURN_IF_FAILURE(budget_consumption_helper_->Run());
    return SuccessExecutionResult();
  }

  ExecutionResult StopComponents() {
    RETURN_IF_FAILURE(budget_consumption_helper_->Stop());
    RETURN_IF_FAILURE(mock_config_provider_->Stop());
    RETURN_IF_FAILURE(io_async_executor_->Stop());
    RETURN_IF_FAILURE(async_executor_->Stop());
    return SuccessExecutionResult();
  }

  std::shared_ptr<spanner_mocks::MockConnection> mock_connection_;
  std::unique_ptr<AsyncExecutorInterface> async_executor_;
  std::unique_ptr<AsyncExecutorInterface> io_async_executor_;
  std::unique_ptr<MockConfigProvider> mock_config_provider_;
  std::unique_ptr<BudgetConsumptionHelper> budget_consumption_helper_;
};

TEST_F(BudgetConsumptionHelperTest, InitializationFailed) {
  std::unique_ptr<BudgetConsumptionHelper> budget_consumption_helper =
      std::make_unique<BudgetConsumptionHelper>(
          mock_config_provider_.get(), async_executor_.get(),
          io_async_executor_.get(), /*spanner_connection=*/nullptr);

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  EXPECT_THAT(
      budget_consumption_helper->Init(),
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_INITIALIZATION_ERROR)));
}

TEST_F(BudgetConsumptionHelperTest, ExecutorNotYetRunShouldFail) {
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  EXPECT_THAT(budget_consumption_helper_->ConsumeBudgets(context),
              ResultIs(FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING)));
}

class BudgetConsumptionHelperWithLifecycleTest
    : public BudgetConsumptionHelperTest {
 protected:
  void SetUp() override {
    BudgetConsumptionHelperTest::SetUp();
    mock_config_provider_->Set(kBudgetKeyTableName, std::string(kTableName));
    ASSERT_SUCCESS(InitAndRunComponents());
  }

  void TearDown() override {
    BudgetConsumptionHelperTest::TearDown();
    ASSERT_SUCCESS(StopComponents());
  }
};

TEST_F(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsOnNonExistingRowShouldSuccess) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource();

  // No row exist in Spanner
  EXPECT_CALL(*source, NextRow()).WillRepeatedly(Return(spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey("fake-key-name", "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budgets.push_back(ConsumeBudgetMetadata{
      std::make_shared<std::string>("fake-key-name"), 1, 3601000000000});
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  spanner::Mutation m =
      cloud::spanner::InsertMutationBuilder(
          std::string(kTableName), {std::string(kBudgetKeySpannerColumnName),
                                    std::string(kTimeframeSpannerColumnName),
                                    std::string(kValueSpannerColumnName)})
          .EmplaceRow(
              "fake-key-name", "0",
              spanner::Json(
                  R"({"TokenCount":"1 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"})"))
          .Build();
  EXPECT_CALL(*mock_connection_,
              Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(spanner::CommitResult{}));

  absl::BlockingCounter blocking(1);
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    blocking.DecrementCount();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  blocking.Wait();

  EXPECT_SUCCESS(result_context.result);
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_F(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsOnExistingRowShouldSuccess) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource();

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("fake-key-name")},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(spanner::Json(
                R"({"TokenCount":"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"})"))}})))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey("fake-key-name", "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budgets.push_back(ConsumeBudgetMetadata{
      std::make_shared<std::string>("fake-key-name"), 1, 3601000000000});
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  spanner::Mutation m =
      cloud::spanner::UpdateMutationBuilder(
          std::string(kTableName), {std::string(kBudgetKeySpannerColumnName),
                                    std::string(kTimeframeSpannerColumnName),
                                    std::string(kValueSpannerColumnName)})
          .EmplaceRow(
              "fake-key-name", "0",
              spanner::Json(
                  R"({"TokenCount":"1 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"})"))
          .Build();
  EXPECT_CALL(*mock_connection_,
              Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(spanner::CommitResult{}));

  absl::BlockingCounter blocking(1);
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    blocking.DecrementCount();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  blocking.Wait();

  EXPECT_SUCCESS(result_context.result);
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_F(BudgetConsumptionHelperWithLifecycleTest, ConsumeBudgetsWithoutBudget) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource();

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("fake-key-name")},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(spanner::Json(
                R"({"TokenCount":"1 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"})"))}})))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey("fake-key-name", "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budgets.push_back(ConsumeBudgetMetadata{
      std::make_shared<std::string>("fake-key-name"), 1, 3601000000000});
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::BlockingCounter blocking(1);
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    blocking.DecrementCount();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  blocking.Wait();

  EXPECT_THAT(result_context.result,
              ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices,
              ElementsAre(0));
}

TEST_F(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsWithInvalidJsonValueColumn) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource();

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("fake-key-name")},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(
                spanner::Json(R"({"TokenCount": Invalid JSON format")"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey("fake-key-name", "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budgets.push_back(ConsumeBudgetMetadata{
      std::make_shared<std::string>("fake-key-name"), 1, 3601000000000});
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::BlockingCounter blocking(1);
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    blocking.DecrementCount();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  blocking.Wait();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_F(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsWithoutTokenCountFieldInJson) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource();

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("fake-key-name")},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(spanner::Json(
                R"({"TokenCountFake": "No TokenCount field"})"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey("fake-key-name", "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budgets.push_back(ConsumeBudgetMetadata{
      std::make_shared<std::string>("fake-key-name"), 1, 3601000000000});
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::BlockingCounter blocking(1);
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    blocking.DecrementCount();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  blocking.Wait();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_F(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsDeserializationFailed) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource();

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("fake-key-name")},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(spanner::Json(
                R"({"TokenCount": "Invalid TokenCount field"})"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(cloud::spanner::MakeKey("fake-key-name", "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budgets.push_back(ConsumeBudgetMetadata{
      std::make_shared<std::string>("fake-key-name"), 1, 3601000000000});
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::BlockingCounter blocking(1);
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    blocking.DecrementCount();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  blocking.Wait();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}
}  // namespace
}  // namespace google::scp::pbs
