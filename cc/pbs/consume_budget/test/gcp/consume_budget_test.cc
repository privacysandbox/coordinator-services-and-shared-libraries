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
#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"

#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/pbs/proto/storage/budget_value.pb.h"
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
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::Values;
namespace spanner = ::google::cloud::spanner;
namespace spanner_mocks = ::google::cloud::spanner_mocks;

constexpr absl::string_view kMigrationPhase1 = "phase_1";
constexpr absl::string_view kMigrationPhase2 = "phase_2";
constexpr absl::string_view kMigrationPhase3 = "phase_3";
constexpr absl::string_view kMigrationPhase4 = "phase_4";

constexpr absl::string_view kBudgetKeySpannerColumnName = "Budget_Key";
constexpr absl::string_view kTimeframeSpannerColumnName = "Timeframe";
constexpr absl::string_view kValueSpannerColumnName = "Value";
constexpr absl::string_view kValueProtoSpannerColumnName = "ValueProto";
constexpr size_t kThreadCount = 5;
constexpr size_t kQueueSize = 100;
constexpr TokenCount kDefaultPrivacyBudgetCount = 1;
constexpr int32_t kDefaultLaplaceDpBudgetCount = 6400;
constexpr int32_t kEmptyBudgetCount = 0;
constexpr size_t kDefaultTokenCountSize = 24;
constexpr absl::string_view kTableName = "fake-table-name";
constexpr absl::string_view kFakeKeyName = "fake-key-name";

constexpr absl::string_view kBudgetKeyTableMetadataPhase1 = R"pb(
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
constexpr absl::string_view kBudgetKeyTableMetadataPhase2Phase3 = R"pb(
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
    fields: {
      name: "ValueProto",
      type: { code: PROTO }
    }
  })pb";
constexpr absl::string_view kBudgetKeyTableMetadataPhase4 = R"pb(
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
      name: "ValueProto",
      type: { code: PROTO }
    }
  })pb";

absl::string_view BudgetKeyTableMetadata(absl::string_view migration_phase) {
  if (migration_phase == kMigrationPhase1) {
    return kBudgetKeyTableMetadataPhase1;
  }
  if (migration_phase == kMigrationPhase4) {
    return kBudgetKeyTableMetadataPhase4;
  }
  return kBudgetKeyTableMetadataPhase2Phase3;
}

std::unique_ptr<spanner_mocks::MockResultSetSource>
CreatePbsMockResultSetSource(absl::string_view migration_phase) {
  auto source =
      std::make_unique<google::cloud::spanner_mocks::MockResultSetSource>();

  google::spanner::v1::ResultSetMetadata metadata;
  EXPECT_TRUE(TextFormat::ParseFromString(
      BudgetKeyTableMetadata(migration_phase), &metadata));
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
    : public BudgetConsumptionHelperTest,
      public testing::WithParamInterface<
          /*migration_phase=*/std::string> {
 protected:
  void SetUp() override {
    BudgetConsumptionHelperTest::SetUp();
    mock_config_provider_->Set(kBudgetKeyTableName, std::string(kTableName));
    mock_config_provider_->Set(kValueProtoMigrationPhase, GetMigrationPhase());
    ASSERT_SUCCESS(InitAndRunComponents());
  }

  void TearDown() override {
    BudgetConsumptionHelperTest::TearDown();
    ASSERT_SUCCESS(StopComponents());
  }

  std::string GetMigrationPhase() { return GetParam(); }

  bool WriteValueColumn() {
    return (GetMigrationPhase() == kMigrationPhase1 ||
            GetMigrationPhase() == kMigrationPhase2 ||
            GetMigrationPhase() == kMigrationPhase3);
  }

  bool WriteValueProtoColumn() {
    return (GetMigrationPhase() == kMigrationPhase2 ||
            GetMigrationPhase() == kMigrationPhase3 ||
            GetMigrationPhase() == kMigrationPhase4);
  }

  bool ReadFromValueColumn() {
    return (GetMigrationPhase() == kMigrationPhase1 ||
            GetMigrationPhase() == kMigrationPhase2);
  }

  std::vector<std::string> GetTableColumns() {
    std::vector<std::string> columns = {
        std::string(kBudgetKeySpannerColumnName),
        std::string(kTimeframeSpannerColumnName),
    };

    if (WriteValueColumn()) {
      columns.emplace_back(kValueSpannerColumnName);
    }
    if (WriteValueProtoColumn()) {
      columns.emplace_back(kValueProtoSpannerColumnName);
    }

    return columns;
  }

  std::vector<spanner::Value> GetTableValues(
      absl::Span<const int64_t> token_count) {
    std::vector<spanner::Value> values = {
        spanner::Value(std::string(kFakeKeyName)),
        spanner::Value("0"),
    };

    if (WriteValueColumn()) {
      std::vector<std::string> token_count_strings;
      std::transform(token_count.begin(), token_count.end(),
                     std::back_inserter(token_count_strings),
                     [](int count) { return std::to_string(count); });
      std::string serialized_token_count =
          absl::StrJoin(token_count_strings, " ");
      std::string json =
          absl::StrFormat(R"({"TokenCount":"%s"})", serialized_token_count);
      values.emplace_back(spanner::Json(json));
    }

    if (WriteValueProtoColumn()) {
      privacy_sandbox_pbs::BudgetValue budget_value;
      budget_value.mutable_laplace_dp_budgets()->mutable_budgets()->Reserve(
          token_count.size());

      privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets* dp_budgets =
          budget_value.mutable_laplace_dp_budgets();
      for (const auto& token : token_count) {
        dp_budgets->add_budgets(token == kDefaultPrivacyBudgetCount
                                    ? kDefaultLaplaceDpBudgetCount
                                    : kEmptyBudgetCount);
      }
      values.emplace_back(
          spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>(
              budget_value));
    }

    return values;
  }

  privacy_sandbox_pbs::BudgetValue GetProtoValueWithInvalidTokens(
      absl::Span<const int64_t> token_count) {
    privacy_sandbox_pbs::BudgetValue budget_value;
    budget_value.mutable_laplace_dp_budgets()->mutable_budgets()->Reserve(
        token_count.size());
    privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets* dp_budgets =
        budget_value.mutable_laplace_dp_budgets();
    for (const auto& token : token_count) {
      dp_budgets->add_budgets(token);
    }
    return budget_value;
  }

  std::vector<std::pair<std::string, spanner::Value>> GetRowPairsForNextRow(
      absl::Span<const int64_t> token_count) {
    std::vector<std::string> columns = GetTableColumns();
    std::vector<spanner::Value> values = GetTableValues(token_count);
    EXPECT_EQ(columns.size(), values.size());

    std::vector<std::pair<std::string, spanner::Value>> zipped_vector;
    for (size_t i = 0; i < columns.size(); ++i) {
      if (ReadFromValueColumn() && columns[i] == kValueProtoSpannerColumnName) {
        continue;
      } else if (!ReadFromValueColumn() &&
                 columns[i] == kValueSpannerColumnName) {
        continue;
      }
      zipped_vector.emplace_back(columns[i], values[i]);
    }

    return zipped_vector;
  }
};

INSTANTIATE_TEST_SUITE_P(BudgetConsumptionHelperWithLifecycleTest,
                         BudgetConsumptionHelperWithLifecycleTest,
                         Values(kMigrationPhase1, kMigrationPhase2,
                                kMigrationPhase3, kMigrationPhase4));

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsOnNonExistingRowShouldSuccess) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  // No row exist in Spanner
  EXPECT_CALL(*source, NextRow()).WillRepeatedly(Return(spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(spanner::RowStream(std::move(source))));

  std::vector<int64_t> token_count = {1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

  spanner::Mutation m = cloud::spanner::InsertMutationBuilder(
                            std::string(kTableName), GetTableColumns())
                            .AddRow(GetTableValues(token_count))
                            .Build();
  EXPECT_CALL(*mock_connection_,
              Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(spanner::CommitResult{}));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_SUCCESS(result_context.result);
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsOnExistingRowShouldSuccess) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  std::vector<int64_t> token_count = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

  EXPECT_CALL(*source, NextRow())
      .WillOnce(
          Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(token_count))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(spanner::RowStream(std::move(source))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  token_count[1] = 0;  // Consuming budget for the concerned hour
  spanner::Mutation expected_mutation =
      cloud::spanner::UpdateMutationBuilder(std::string(kTableName),
                                            GetTableColumns())
          .AddRow(GetTableValues(token_count))
          .Build();
  EXPECT_CALL(*mock_connection_,
              Commit(FieldsAre(_, UnorderedElementsAre(expected_mutation), _)))
      .WillOnce(Return(spanner::CommitResult{}));

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_SUCCESS(result_context.result);
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest, ConsumeBudgetsWithoutBudget) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          GetRowPairsForNextRow({1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_THAT(result_context.result,
              ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices,
              ElementsAre(0));
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsWithInvalidJsonValueColumn) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value(std::string(kFakeKeyName))},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(
                spanner::Json(R"({"TokenCount": Invalid JSON format")"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsWithoutTokenCountFieldInJson) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value(std::string(kFakeKeyName))},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(spanner::Json(
                R"({"TokenCountFake": "No TokenCount field"})"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source)))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsDeserializationFailed) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value(std::string(kFakeKeyName))},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueSpannerColumnName),
            spanner::Value(spanner::Json(
                R"({"TokenCount": "Invalid TokenCount field"})"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(
      cloud::spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(spanner::RowStream(std::move(source))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsWithNoLaplaceDp) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value(std::string(kFakeKeyName))},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueProtoSpannerColumnName),
            spanner::Value(
                spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>(
                    privacy_sandbox_pbs::BudgetValue()))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(
      cloud::spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(spanner::RowStream(std::move(source))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsWithInvalidLaplaceDpSize) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value(std::string(kFakeKeyName))},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueProtoSpannerColumnName),
            spanner::Value(
                spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>(
                    GetProtoValueWithInvalidTokens({1, 1, 1})))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(
      cloud::spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(spanner::RowStream(std::move(source))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}

TEST_P(BudgetConsumptionHelperWithLifecycleTest,
       ConsumeBudgetsWithInvalidLaplaceDpTokens) {
  std::unique_ptr<spanner_mocks::MockResultSetSource> source =
      CreatePbsMockResultSetSource(GetMigrationPhase());

  std::vector<int64_t> token_count(kDefaultTokenCountSize,
                                   kDefaultLaplaceDpBudgetCount);
  token_count[0] -= 1;  // Making an invalid entry

  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value(std::string(kFakeKeyName))},
           {std::string(kTimeframeSpannerColumnName), spanner::Value("0")},
           {std::string(kValueProtoSpannerColumnName),
            spanner::Value(
                spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>(
                    GetProtoValueWithInvalidTokens(token_count)))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(
      cloud::spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(spanner::RowStream(std::move(source))));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  ConsumeBudgetMetadata consume_budget_metadata{
      .budget_key_name = std::make_shared<std::string>(kFakeKeyName),
      .token_count = 1,
      .time_bucket = 3601000000000};
  context.request->budgets.push_back(consume_budget_metadata);
  context.response = std::make_shared<ConsumeBudgetsResponse>();

  EXPECT_CALL(*mock_connection_, Commit).Times(0);
  EXPECT_CALL(*mock_connection_, Rollback).Times(1);

  absl::Notification notification;
  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> result_context;
  context.callback = [&](AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>& context) {
    result_context = context;
    notification.Notify();
  };
  EXPECT_SUCCESS(budget_consumption_helper_->ConsumeBudgets(context));
  notification.WaitForNotification();

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices, IsEmpty());
}
}  // namespace
}  // namespace google::scp::pbs
