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
#include "cc/core/async_executor/src/error_codes.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/interface/http_types.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/pbs/proto/storage/budget_value.pb.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "gmock/gmock.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/protobuf/text_format.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace google::scp::pbs {
namespace {

using ::google::protobuf::TextFormat;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::test::ResultIs;
using ::google::scp::pbs::kBudgetKeyTableName;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_EXHAUSTED;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_FAIL_TO_COMMIT;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_INITIALIZATION_ERROR;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_PARSING_ERROR;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::AsyncExecutor;
using ::privacy_sandbox::pbs_common::AsyncExecutorInterface;
using ::privacy_sandbox::pbs_common::AuthContext;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::MockConfigProvider;
using ::privacy_sandbox::pbs_common::SC_ASYNC_EXECUTOR_NOT_RUNNING;
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

class MockBudgetConsumer : public BudgetConsumer {
 public:
  MOCK_METHOD(core::ExecutionResult, ParseTransactionRequest,
              (const AuthContext&, const HttpHeaders&, const nlohmann::json&),
              (override));
  MOCK_METHOD(core::ExecutionResult, ParseTransactionRequest,
              (const AuthContext&, const HttpHeaders&,
               const ConsumePrivacyBudgetRequest&),
              (override));
  MOCK_METHOD(spanner::KeySet, GetSpannerKeySet, (), (override));
  MOCK_METHOD(size_t, GetKeyCount, (), (override));
  MOCK_METHOD(core::ExecutionResultOr<std::vector<std::string>>, GetReadColumns,
              (), (override));
  MOCK_METHOD(std::vector<std::string>, DebugKeyList, (), (override));
  MOCK_METHOD(SpannerMutationsResult, ConsumeBudget,
              (spanner::RowStream&, absl::string_view), (override));
};

template <bool should_enable_budget_consumer>
class BudgetConsumptionHelperWithLifecycleTest
    : public BudgetConsumptionHelperTest,
      public testing::WithParamInterface<
          /*migration_phase=*/std::string> {
 protected:
  void SetUp() override {
    BudgetConsumptionHelperTest::SetUp();
    source_ = CreatePbsMockResultSetSource(GetMigrationPhase());
    mock_config_provider_->Set(kBudgetKeyTableName, std::string(kTableName));
    mock_config_provider_->Set(kValueProtoMigrationPhase, GetMigrationPhase());
    mock_config_provider_->SetBool(kEnableBudgetConsumerMigration,
                                   should_enable_budget_consumer);
    ASSERT_SUCCESS(InitAndRunComponents());

    if (should_enable_budget_consumer) {
      mock_budget_consumer_ = std::make_unique<MockBudgetConsumer>();
    }
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

  std::vector<std::string> GetReadColumns() {
    std::vector<std::string> columns = {
        std::string(kBudgetKeySpannerColumnName),
        std::string(kTimeframeSpannerColumnName),
    };
    if (ReadFromValueColumn()) {
      columns.emplace_back(kValueSpannerColumnName);
    } else {
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

  std::unique_ptr<spanner_mocks::MockResultSetSource> source_;
  std::unique_ptr<MockBudgetConsumer> mock_budget_consumer_;
};

using BudgetConsumptionHelperWithLifecycleTestWithBudgetConsumer =
    BudgetConsumptionHelperWithLifecycleTest<true>;
using BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer =
    BudgetConsumptionHelperWithLifecycleTest<false>;

INSTANTIATE_TEST_SUITE_P(
    BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
    BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
    Values(kMigrationPhase1, kMigrationPhase2, kMigrationPhase3,
           kMigrationPhase4));

INSTANTIATE_TEST_SUITE_P(
    BudgetConsumptionHelperWithLifecycleTestWithBudgetConsumer,
    BudgetConsumptionHelperWithLifecycleTestWithBudgetConsumer,
    Values(kMigrationPhase1, kMigrationPhase2, kMigrationPhase3,
           kMigrationPhase4));

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsOnNonExistingRowShouldSuccess) {
  // No row exist in Spanner
  EXPECT_CALL(*source_, NextRow()).WillRepeatedly(Return(spanner::Row()));

  spanner::KeySet expected_key_set;
  expected_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));
  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(expected_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsOnExistingRowShouldSuccess) {
  std::vector<int64_t> token_count = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsWithoutBudget) {
  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source_)))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsWithInvalidJsonValueColumn) {
  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source_)))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsWithoutTokenCountFieldInJson) {
  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(ByMove(spanner::RowStream(std::move(source_)))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsDeserializationFailed) {
  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsWithNoLaplaceDp) {
  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsWithInvalidLaplaceDpSize) {
  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithoutBudgetConsumer,
       ConsumeBudgetsWithInvalidLaplaceDpTokens) {
  std::vector<int64_t> token_count(kDefaultTokenCountSize,
                                   kDefaultLaplaceDpBudgetCount);
  token_count[0] -= 1;  // Making an invalid entry

  EXPECT_CALL(*source_, NextRow())
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
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithBudgetConsumer,
       SuccessWithValidSpannerMutations) {
  std::vector<int64_t> token_count(kDefaultTokenCountSize,
                                   kDefaultPrivacyBudgetCount);

  EXPECT_CALL(*mock_budget_consumer_, GetReadColumns())
      .Times(1)
      .WillOnce(Return(GetReadColumns()));

  spanner::KeySet mock_key_set;
  mock_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));

  EXPECT_CALL(*mock_budget_consumer_, GetSpannerKeySet())
      .Times(1)
      .WillOnce(Return(mock_key_set));

  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(mock_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .Times(1)
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

  token_count[0] = kEmptyBudgetCount;
  spanner::Mutation mock_mutation =
      cloud::spanner::UpdateMutationBuilder(std::string(kTableName),
                                            GetTableColumns())
          .AddRow(GetTableValues(token_count))
          .Build();

  spanner::RowStream received_row_stream;
  EXPECT_CALL(*mock_budget_consumer_, ConsumeBudget(_, Eq(kTableName)))
      .Times(1)
      .WillOnce(Return(SpannerMutationsResult{
          .status = cloud::Status(),
          .execution_result = SuccessExecutionResult(),
          .budget_exhausted_indices = std::vector<size_t>(),
          .mutations = spanner::Mutations{mock_mutation},
      }));

  EXPECT_CALL(*mock_connection_,
              Commit(FieldsAre(_, UnorderedElementsAre(mock_mutation), _)))
      .WillOnce(Return(spanner::CommitResult{}));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budget_consumer = std::move(mock_budget_consumer_);
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

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithBudgetConsumer,
       FailureWithBudgetExhaustedIndices) {
  EXPECT_CALL(*mock_budget_consumer_, GetReadColumns())
      .Times(1)
      .WillOnce(Return(GetReadColumns()));

  spanner::KeySet mock_key_set;
  mock_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));

  EXPECT_CALL(*mock_budget_consumer_, GetSpannerKeySet())
      .Times(1)
      .WillOnce(Return(mock_key_set));

  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(mock_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .Times(1)
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

  spanner::RowStream received_row_stream;
  EXPECT_CALL(*mock_budget_consumer_, ConsumeBudget(_, Eq(kTableName)))
      .Times(1)
      .WillOnce(Return(SpannerMutationsResult{
          .status = cloud::Status(cloud::StatusCode::kInvalidArgument,
                                  "Not enough budget."),
          .execution_result =
              FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED),
          .budget_exhausted_indices = {1, 3, 7},
          .mutations = spanner::Mutations{},
      }));

  EXPECT_CALL(*mock_connection_, Commit(FieldsAre(_, _, _))).Times(0);

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budget_consumer = std::move(mock_budget_consumer_);
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

  EXPECT_THAT(result_context.result,
              ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED)));
  EXPECT_THAT(result_context.response->budget_exhausted_indices,
              ElementsAre(1, 3, 7));
}

TEST_P(BudgetConsumptionHelperWithLifecycleTestWithBudgetConsumer,
       FailedToCommit) {
  std::vector<int64_t> token_count(kDefaultTokenCountSize,
                                   kDefaultPrivacyBudgetCount);

  EXPECT_CALL(*mock_budget_consumer_, GetReadColumns())
      .Times(1)
      .WillOnce(Return(GetReadColumns()));

  spanner::KeySet mock_key_set;
  mock_key_set.AddKey(spanner::MakeKey(std::string(kFakeKeyName), "0"));

  EXPECT_CALL(*mock_budget_consumer_, GetSpannerKeySet())
      .Times(1)
      .WillOnce(Return(mock_key_set));

  EXPECT_CALL(
      *mock_connection_,
      Read(AllOf(
          Field(&spanner::Connection::ReadParams::keys, Eq(mock_key_set)),
          Field(&spanner::Connection::ReadParams::table, Eq(kTableName)))))
      .Times(1)
      .WillOnce(Return(spanner::RowStream(std::move(source_))));

  token_count[0] = kEmptyBudgetCount;
  spanner::Mutation mock_mutation =
      cloud::spanner::UpdateMutationBuilder(std::string(kTableName),
                                            GetTableColumns())
          .AddRow(GetTableValues(token_count))
          .Build();

  spanner::RowStream received_row_stream;
  EXPECT_CALL(*mock_budget_consumer_, ConsumeBudget(_, Eq(kTableName)))
      .Times(1)
      .WillOnce(Return(SpannerMutationsResult{
          .status = cloud::Status(),
          .execution_result = SuccessExecutionResult(),
          .budget_exhausted_indices = std::vector<size_t>(),
          .mutations = spanner::Mutations{mock_mutation},
      }));

  EXPECT_CALL(*mock_connection_,
              Commit(FieldsAre(_, UnorderedElementsAre(mock_mutation), _)))
      .WillOnce(Return(cloud::Status(cloud::StatusCode::kPermissionDenied,
                                     "PermissionDenied")));

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse> context;
  context.request = std::make_shared<ConsumeBudgetsRequest>();
  context.request->budget_consumer = std::move(mock_budget_consumer_);
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

  EXPECT_THAT(
      result_context.result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_FAIL_TO_COMMIT)));
  EXPECT_TRUE(result_context.response->budget_exhausted_indices.empty());
}
}  // namespace
}  // namespace google::scp::pbs
