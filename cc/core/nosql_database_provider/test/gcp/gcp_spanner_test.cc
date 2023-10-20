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

#include "core/nosql_database_provider/src/gcp/gcp_spanner.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/interface/logger_interface.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/status.h"
#include "google/spanner/v1/spanner.pb.h"
#include "public/core/test/interface/execution_result_matchers.h"

namespace google::scp::core::test {
namespace {

using google::cloud::Status;
using google::cloud::spanner::Client;
using google::cloud::spanner::CommitResult;
using google::cloud::spanner::DmlResult;
using google::cloud::spanner::Json;
using google::cloud::spanner::MakeInsertOrUpdateMutation;
using google::cloud::spanner::Mutation;
using google::cloud::spanner::Row;
using google::cloud::spanner::RowStream;
using google::cloud::spanner::SqlStatement;
using google::cloud::spanner::Transaction;
using google::cloud::spanner::Value;
using google::cloud::spanner_mocks::MakeRow;
using google::cloud::spanner_mocks::MockConnection;
using google::cloud::spanner_mocks::MockResultSetSource;
using google::scp::core::LoggerInterface;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::GlobalLogger;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::Logger;
using google::scp::core::nosql_database_provider::GcpSpanner;
using google::spanner::v1::ResultSetStats;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::move;
using std::optional;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using testing::_;
using testing::A;
using testing::ByMove;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::FieldsAre;
using testing::IsEmpty;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Pointee;
using testing::PrintToString;
using testing::Return;
using testing::StrEq;
using testing::UnorderedElementsAre;
using testing::VariantWith;

class TestGcpSpanner : public GcpSpanner {
 public:
  explicit TestGcpSpanner(
      std::shared_ptr<Client> spanner_client,
      std::shared_ptr<AsyncExecutorInterface> async_executor,
      std::unique_ptr<unordered_map<string, pair<string, optional<string>>>>
          table_name_to_keys)
      : GcpSpanner(spanner_client, async_executor, move(table_name_to_keys),
                   AsyncPriority::Normal, AsyncPriority::Normal) {}
};

MATCHER_P(SqlEqual, expected_sql, "") {
  string no_whitespace_arg_sql = arg.statement.sql();
  absl::RemoveExtraAsciiWhitespace(&no_whitespace_arg_sql);
  string no_whitespace_expected_sql = expected_sql.sql();
  absl::RemoveExtraAsciiWhitespace(&no_whitespace_expected_sql);

  SqlStatement modified_arg(no_whitespace_arg_sql, arg.statement.params());
  SqlStatement modified_expected(no_whitespace_expected_sql,
                                 expected_sql.params());

  if (!ExplainMatchResult(Eq(modified_expected), modified_arg,
                          result_listener)) {
    string actual =
        absl::StrFormat("Actual - SQL: \"%s\"", no_whitespace_arg_sql);
    for (const auto& [name, val] : modified_arg.params()) {
      absl::StrAppend(&actual, absl::StrFormat("\n[param]: {%s=%s}", name,
                                               PrintToString(val)));
    }
    *result_listener << actual;
    return false;
  }
  return true;
}

constexpr char kBudgetKeyTableName[] = "BudgetKeys";
constexpr char kBudgetKeyPartitionKeyName[] = "BudgetKeyId";
constexpr char kBudgetKeySortKeyName[] = "Timeframe";

constexpr char kPartitionLockTableName[] = "PartitionLock";
constexpr char kPartitionLockPartitionKeyName[] = "LockId";

std::unique_ptr<unordered_map<string, pair<string, optional<string>>>>
GetTableNameToKeysMap() {
  auto map =
      make_unique<unordered_map<string, pair<string, optional<string>>>>();
  map->emplace(kBudgetKeyTableName,
               make_pair(kBudgetKeyPartitionKeyName, kBudgetKeySortKeyName));
  map->emplace(kPartitionLockTableName,
               make_pair(kPartitionLockPartitionKeyName, std::nullopt));
  return map;
}

class GcpSpannerTests : public testing::Test {
 protected:
  static void SetUpTestSuite() { TestLoggingUtils::EnableLogOutputToConsole(); }

  GcpSpannerTests()
      : connection_(make_shared<NiceMock<MockConnection>>()),
        gcp_spanner_(make_shared<Client>(connection_),
                     make_shared<MockAsyncExecutor>(),
                     GetTableNameToKeysMap()) {
    NoSqlDatabaseKeyValuePair partition_key{
        make_shared<NoSQLDatabaseAttributeName>(),
        make_shared<NoSQLDatabaseValidAttributeValueTypes>()};

    GetDatabaseItemRequest get_request;
    get_request.table_name = make_shared<string>(kBudgetKeyTableName);
    get_request.partition_key =
        make_shared<NoSqlDatabaseKeyValuePair>(partition_key);

    get_database_item_context_.request =
        make_shared<GetDatabaseItemRequest>(move(get_request));

    get_database_item_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    UpsertDatabaseItemRequest upsert_request;
    upsert_request.table_name = make_shared<string>(kBudgetKeyTableName);
    upsert_request.partition_key =
        make_shared<NoSqlDatabaseKeyValuePair>(partition_key);

    upsert_request.new_attributes =
        make_shared<vector<NoSqlDatabaseKeyValuePair>>();

    upsert_database_item_context_.request =
        make_shared<UpsertDatabaseItemRequest>(move(upsert_request));

    upsert_database_item_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    ON_CALL(*connection_, Commit).WillByDefault(Return(CommitResult{}));
  }

  ResultSetStats OneRowStats() {
    ResultSetStats stats;
    stats.set_row_count_exact(1);
    return stats;
  }

  ResultSetStats ZeroRowStats() {
    ResultSetStats stats;
    stats.set_row_count_exact(0);
    return stats;
  }

  shared_ptr<MockConnection> connection_;
  TestGcpSpanner gcp_spanner_;

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context_;

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  std::atomic_bool finish_called_{false};
};

TEST_F(GcpSpannerTests, GetItemWithPartitionKeyOnly) {
  *get_database_item_context_.request->table_name = kPartitionLockTableName;
  *get_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *get_database_item_context_.request->partition_key->attribute_value = "3";

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM PartitionLock WHERE LockId = "
      "@partition_key";
  SqlStatement::ParamType expected_params;
  expected_params.emplace("partition_key", "3");
  SqlStatement sql(std::move(expected_query), expected_params);

  auto returned_row = MakeRow(Json(R"""(
    {
      "token_count": "1"
    }
  )"""));
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(sql)))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_database_item_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    const auto& response = context.response;
    ASSERT_THAT(response, NotNull());
    EXPECT_THAT(
        response->partition_key,
        Pointee(FieldsAre(Pointee(StrEq(kPartitionLockPartitionKeyName)),
                          Pointee(VariantWith<string>("3")))));
    EXPECT_THAT(response->sort_key, IsNull());
    EXPECT_THAT(response->attributes, Pointee(UnorderedElementsAre(FieldsAre(
                                          Pointee(StrEq("token_count")),
                                          Pointee(VariantWith<string>("1"))))));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_spanner_.GetDatabaseItem(get_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemWithPartitionAndSortKey) {
  *get_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *get_database_item_context_.request->partition_key->attribute_value = "3";

  get_database_item_context_.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(NoSqlDatabaseKeyValuePair{
          make_shared<NoSQLDatabaseAttributeName>(kBudgetKeySortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("2")});

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM BudgetKeys WHERE BudgetKeyId = "
      "@partition_key AND Timeframe = @sort_key";
  SqlStatement::ParamType expected_params;
  expected_params.emplace("partition_key", "3");
  expected_params.emplace("sort_key", "2");
  SqlStatement sql(std::move(expected_query), expected_params);

  auto returned_row = MakeRow(Json(R"""(
    {
      "token_count": "1"
    }
  )"""));
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(sql)))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_database_item_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    const auto& response = context.response;
    ASSERT_THAT(response, NotNull());
    EXPECT_THAT(response->partition_key,
                Pointee(FieldsAre(Pointee(StrEq(kBudgetKeyPartitionKeyName)),
                                  Pointee(VariantWith<string>("3")))));
    EXPECT_THAT(response->sort_key,
                Pointee(FieldsAre(Pointee(StrEq(kBudgetKeySortKeyName)),
                                  Pointee(VariantWith<string>("2")))));
    EXPECT_THAT(response->attributes, Pointee(UnorderedElementsAre(FieldsAre(
                                          Pointee(StrEq("token_count")),
                                          Pointee(VariantWith<string>("1"))))));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_spanner_.GetDatabaseItem(get_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemWithPartitionAndSortKeyWithAttributes) {
  *get_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *get_database_item_context_.request->partition_key->attribute_value = "3";

  get_database_item_context_.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(NoSqlDatabaseKeyValuePair{
          make_shared<NoSQLDatabaseAttributeName>(kBudgetKeySortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("2")});
  get_database_item_context_.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  get_database_item_context_.request->attributes->emplace_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name =
              make_shared<NoSQLDatabaseAttributeName>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM BudgetKeys WHERE BudgetKeyId = "
      "@partition_key AND Timeframe = @sort_key AND JSON_VALUE(Value, "
      "'$.token_count') = @attribute_0";
  SqlStatement::ParamType expected_params;
  expected_params.emplace("partition_key", "3");
  expected_params.emplace("sort_key", "2");
  expected_params.emplace("attribute_0", "1");
  SqlStatement sql(std::move(expected_query), expected_params);

  auto returned_row = MakeRow(Json(R"""(
    {
      "token_count": "1"
    }
  )"""));
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(sql)))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_database_item_context_.callback = [this](auto& context) {
    const auto& response = context.response;
    ASSERT_THAT(response, NotNull());
    EXPECT_THAT(response->partition_key,
                Pointee(FieldsAre(Pointee(StrEq(kBudgetKeyPartitionKeyName)),
                                  Pointee(VariantWith<string>("3")))));
    EXPECT_THAT(response->sort_key, Pointee(_));
    EXPECT_THAT(response->attributes, Pointee(UnorderedElementsAre(FieldsAre(
                                          Pointee(StrEq("token_count")),
                                          Pointee(VariantWith<string>("1"))))));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_spanner_.GetDatabaseItem(get_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemNoRowFound) {
  *get_database_item_context_.request->table_name = kPartitionLockTableName;
  *get_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *get_database_item_context_.request->partition_key->attribute_value = "3";

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM PartitionLock WHERE LockId = "
      "@partition_key";
  SqlStatement::ParamType expected_params;
  expected_params.emplace("partition_key", "3");
  SqlStatement sql(std::move(expected_query), expected_params);

  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow).WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(sql)))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_database_item_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::FailureExecutionResult(
                    errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)));
    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_spanner_.GetDatabaseItem(get_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemJsonParseFail) {
  *get_database_item_context_.request->table_name = kPartitionLockTableName;
  *get_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *get_database_item_context_.request->partition_key->attribute_value = "3";

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM PartitionLock WHERE LockId = "
      "@partition_key";
  SqlStatement::ParamType expected_params;
  expected_params.emplace("partition_key", "3");
  SqlStatement sql(std::move(expected_query), expected_params);

  // Put an integer where a Json is expected to cause failure.
  auto returned_row = MakeRow(1);
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(sql)))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_database_item_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::RetryExecutionResult(
                    errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(gcp_spanner_.GetDatabaseItem(get_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemFailsIfBadPartitionKey) {
  *get_database_item_context_.request->partition_key->attribute_name =
      "SomeOtherKey";
  *get_database_item_context_.request->partition_key->attribute_value = "3";

  auto result = gcp_spanner_.GetDatabaseItem(get_database_item_context_);
  EXPECT_EQ(result,
            core::FailureExecutionResult(
                errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME))
      << GetErrorMessage(result.status_code);

  EXPECT_FALSE(finish_called_);
}

TEST_F(GcpSpannerTests, GetItemFailsIfBadSortKey) {
  *get_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *get_database_item_context_.request->partition_key->attribute_value = "3";
  // Sort key is bad because it's absent.

  EXPECT_THAT(gcp_spanner_.GetDatabaseItem(get_database_item_context_),
              ResultIs(core::FailureExecutionResult(
                  errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME)));

  EXPECT_FALSE(finish_called_);
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesWithPartitionKeyOnly) {
  *upsert_database_item_context_.request->table_name = kPartitionLockTableName;
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  SqlStatement expected_sql(
      "SELECT Value FROM PartitionLock WHERE LockId = @partition_key",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow).WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(expected_sql)))
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  Mutation m = MakeInsertOrUpdateMutation(
      kPartitionLockTableName, {kPartitionLockPartitionKeyName, "Value"},
      Value("3"), Value(Json("{\"token_count\":\"1\"}")));
  EXPECT_CALL(*connection_, Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(CommitResult{}));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesWithSortKey) {
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(NoSqlDatabaseKeyValuePair{
          make_shared<NoSQLDatabaseAttributeName>(kBudgetKeySortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("2")});

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  SqlStatement expected_sql(
      "SELECT Value FROM BudgetKeys WHERE BudgetKeyId = @partition_key AND "
      "Timeframe = @sort_key",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow).WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(expected_sql)))
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  Mutation m = MakeInsertOrUpdateMutation(
      kBudgetKeyTableName,
      {kBudgetKeyPartitionKeyName, kBudgetKeySortKeyName, "Value"}, Value("3"),
      Value("2"), Value(Json("{\"token_count\":\"1\"}")));
  EXPECT_CALL(*connection_, Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(CommitResult{}));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesWithExistingValue) {
  *upsert_database_item_context_.request->table_name = kPartitionLockTableName;
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  SqlStatement expected_sql(
      "SELECT Value FROM PartitionLock WHERE LockId = @partition_key",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  // We return a JSON with "other_val" and "token_count" existing, token_count
  // should be overridden.
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(MakeRow(Json(R"""(
    {
      "other_val": "10",
      "token_count": "999"
    }
  )"""))))
      .WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(expected_sql)))
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  Mutation m = MakeInsertOrUpdateMutation(
      kPartitionLockTableName, {kPartitionLockPartitionKeyName, "Value"},
      Value("3"), Value(Json("{\"other_val\":\"10\",\"token_count\":\"1\"}")));
  EXPECT_CALL(*connection_, Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(CommitResult{}));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesFailsIfCommitFails) {
  *upsert_database_item_context_.request->table_name = kPartitionLockTableName;
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(MakeRow(Json(R"""(
    {
      "token_count": "1"
    }
  )"""))))
      .WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery)
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  EXPECT_CALL(*connection_, Commit)
      .WillOnce(Return(Status(google::cloud::StatusCode::kInternal, "Error")));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::RetryExecutionResult(
                    errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesFailsIfJsonParseFails) {
  *upsert_database_item_context_.request->table_name = kPartitionLockTableName;
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  auto returned_results = make_unique<MockResultSetSource>();
  // Place an integer where a JSON is expected to cause failure.
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(MakeRow(1)))
      .WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery)
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  EXPECT_CALL(*connection_, Commit);

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::FailureExecutionResult(
                    errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_CORRUPTED)));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesWithPartitionKeyOnly) {
  *upsert_database_item_context_.request->table_name = kPartitionLockTableName;
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kPartitionLockPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("100")});

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM PartitionLock WHERE LockId = @partition_key AND "
      "JSON_VALUE(Value, '$.token_count') = @attribute_0",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  auto returned_row = MakeRow(Json(R"""(
    {
      "token_count": "100"
    }
  )"""));
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(expected_sql)))
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  Mutation m = MakeInsertOrUpdateMutation(
      kPartitionLockTableName, {kPartitionLockPartitionKeyName, "Value"},
      Value("3"), Value(Json("{\"token_count\":\"1\"}")));
  EXPECT_CALL(*connection_, Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(CommitResult{}));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesWithSortKey) {
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(NoSqlDatabaseKeyValuePair{
          make_shared<NoSQLDatabaseAttributeName>(kBudgetKeySortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("2")});

  upsert_database_item_context_.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("100")});

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM BudgetKeys WHERE BudgetKeyId = @partition_key AND "
      "Timeframe = @sort_key AND JSON_VALUE(Value, '$.token_count') = "
      "@attribute_0",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  auto returned_row = MakeRow(Json(R"""(
    {
      "token_count": "100"
    }
  )"""));
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(expected_sql)))
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  Mutation m = MakeInsertOrUpdateMutation(
      kBudgetKeyTableName,
      {kBudgetKeyPartitionKeyName, kBudgetKeySortKeyName, "Value"}, Value("3"),
      Value("2"), Value(Json("{\"token_count\":\"1\"}")));
  EXPECT_CALL(*connection_, Commit(FieldsAre(_, UnorderedElementsAre(m), _)))
      .WillOnce(Return(CommitResult{}));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesFailsIfNoRowsFound) {
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(NoSqlDatabaseKeyValuePair{
          make_shared<NoSQLDatabaseAttributeName>(kBudgetKeySortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("2")});

  upsert_database_item_context_.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("100")});

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM BudgetKeys WHERE BudgetKeyId = @partition_key AND "
      "Timeframe = @sort_key AND JSON_VALUE(Value, '$.token_count') = "
      "@attribute_0",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  // Return(Row()) means no rows are found.
  EXPECT_CALL(*returned_results, NextRow).WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(expected_sql)))
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  EXPECT_CALL(*connection_, Commit(FieldsAre(_, IsEmpty(), _)))
      .WillOnce(Return(CommitResult{}));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::FailureExecutionResult(
                    errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesFailsIfCommitFails) {
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  upsert_database_item_context_.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(NoSqlDatabaseKeyValuePair{
          make_shared<NoSQLDatabaseAttributeName>(kBudgetKeySortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("2")});

  upsert_database_item_context_.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("100")});

  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")});

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM BudgetKeys WHERE BudgetKeyId = @partition_key AND "
      "Timeframe = @sort_key AND JSON_VALUE(Value, '$.token_count') = "
      "@attribute_0",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  auto returned_row = MakeRow(Json(R"""(
    {
      "token_count": "100"
    }
  )"""));
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));
  EXPECT_CALL(*connection_, ExecuteQuery(SqlEqual(expected_sql)))
      .WillOnce(Return(ByMove(RowStream(move(returned_results)))));

  EXPECT_CALL(*connection_, Commit)
      .WillOnce(Return(Status(google::cloud::StatusCode::kInternal, "error")));

  upsert_database_item_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(core::RetryExecutionResult(
                    errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemFailsIfBadPartitionKey) {
  *upsert_database_item_context_.request->partition_key->attribute_name =
      "SomeOtherKey";
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";

  EXPECT_THAT(
      gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
      ResultIs(core::FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME)));

  EXPECT_FALSE(finish_called_);
}

TEST_F(GcpSpannerTests, UpsertItemFailsIfBadSortKey) {
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kBudgetKeyPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "3";
  // Sort key is bad because it's absent.

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              ResultIs(core::FailureExecutionResult(
                  errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME)));

  EXPECT_FALSE(finish_called_);
}

}  // namespace
}  // namespace google::scp::core::test
