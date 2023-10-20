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

#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_spanner_client_provider.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "cpio/common/src/gcp/error_codes.h"
#include "google/cloud/spanner/admin/mocks/mock_database_admin_connection.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/status.h"
#include "google/spanner/v1/spanner.pb.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::cloud::make_ready_future;
using google::cloud::Status;
using google::cloud::StatusOr;
using google::cloud::spanner::Client;
using google::cloud::spanner::CommitResult;
using google::cloud::spanner::Json;
using google::cloud::spanner::MakeInsertOrUpdateMutation;
using google::cloud::spanner::Mutation;
using google::cloud::spanner::Row;
using google::cloud::spanner::RowStream;
using google::cloud::spanner::SqlStatement;
using google::cloud::spanner::Value;
using google::cloud::spanner_admin::DatabaseAdminClient;
using google::cloud::spanner_admin_mocks::MockDatabaseAdminConnection;
using google::cloud::spanner_mocks::MakeRow;
using google::cloud::spanner_mocks::MockConnection;
using google::cloud::spanner_mocks::MockResultSetSource;
using google::cmrt::sdk::nosql_database_service::v1::CreateTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::CreateTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::ItemAttribute;
using google::cmrt::sdk::nosql_database_service::v1::ItemKey;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_EMPTY_TABLE_NAME;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_CORRUPTED;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_UNSET_KEY_TYPE;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::PartitionAndSortKey;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata;
using google::spanner::admin::database::v1::UpdateDatabaseDdlRequest;
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
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::FieldsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::NotNull;
using testing::PrintToString;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {

constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr char kBudgetKeyTableName[] = "BudgetKeys";
constexpr char kBudgetKeyPartitionKeyName[] = "BudgetKeyId";
constexpr char kBudgetKeySortKeyName[] = "Timeframe";

constexpr char kPartitionLockTableName[] = "PartitionLock";
constexpr char kPartitionLockPartitionKeyName[] = "LockId";

std::unique_ptr<unordered_map<string, PartitionAndSortKey>>
GetTableNameToKeysMap() {
  auto map = make_unique<unordered_map<string, PartitionAndSortKey>>();
  PartitionAndSortKey budget_key_pair;
  budget_key_pair.SetPartitionKey(kBudgetKeyPartitionKeyName);
  budget_key_pair.SetSortKey(kBudgetKeySortKeyName);
  PartitionAndSortKey partition_lock_pair;
  partition_lock_pair.SetPartitionKey(kPartitionLockPartitionKeyName);
  partition_lock_pair.SetNoSortKey();
  map->emplace(kBudgetKeyTableName, move(budget_key_pair));
  map->emplace(kPartitionLockTableName, move(partition_lock_pair));
  return map;
}

ItemAttribute MakeStringAttribute(const string& name, const string& value) {
  ItemAttribute attribute;
  attribute.set_name(name);
  attribute.set_value_string(value);
  return attribute;
}

ItemAttribute MakeIntegerAttribute(const string& name, int32_t value) {
  ItemAttribute attribute;
  attribute.set_name(name);
  attribute.set_value_int(value);
  return attribute;
}

auto ReturnEmptyUpdateMetadata(UpdateDatabaseDdlRequest) {
  return make_ready_future(StatusOr(UpdateDatabaseDdlMetadata()));
}

}  // namespace

namespace google::scp::cpio::client_providers::test {

class MockSpannerFactory : public SpannerFactory {
 public:
  MOCK_METHOD((ExecutionResultOr<
                  pair<shared_ptr<Client>, shared_ptr<DatabaseAdminClient>>>),
              CreateClients, (const string&, const string&, const string&),
              (noexcept, override));
};

class GcpSpannerTests : public testing::Test {
 protected:
  GcpSpannerTests()
      : instance_client_(make_shared<NiceMock<MockInstanceClientProvider>>()),
        connection_(make_shared<NiceMock<MockConnection>>()),
        database_connection_(
            make_shared<NiceMock<MockDatabaseAdminConnection>>()),
        spanner_factory_(make_shared<NiceMock<MockSpannerFactory>>()),
        gcp_spanner_(
            make_shared<NoSQLDatabaseClientOptions>(NoSQLDatabaseClientOptions{
                "instance", "database", GetTableNameToKeysMap()}),
            instance_client_, make_shared<MockAsyncExecutor>(),
            make_shared<MockAsyncExecutor>(), spanner_factory_) {
    instance_client_->instance_resource_name = kInstanceResourceName;
    CreateTableRequest create_table_request;
    create_table_request.mutable_key()->set_table_name(kBudgetKeyTableName);

    create_table_context_.request =
        make_shared<CreateTableRequest>(move(create_table_request));

    create_table_context_.callback = [this](auto) { finish_called_ = true; };

    DeleteTableRequest delete_table_request;
    delete_table_request.set_table_name(kBudgetKeyTableName);

    delete_table_context_.request =
        make_shared<DeleteTableRequest>(move(delete_table_request));

    delete_table_context_.callback = [this](auto) { finish_called_ = true; };

    GetDatabaseItemRequest get_request;
    get_request.mutable_key()->set_table_name(kBudgetKeyTableName);

    get_database_item_context_.request =
        make_shared<GetDatabaseItemRequest>(move(get_request));

    get_database_item_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    UpsertDatabaseItemRequest upsert_request;
    upsert_request.mutable_key()->set_table_name(kBudgetKeyTableName);

    upsert_database_item_context_.request =
        make_shared<UpsertDatabaseItemRequest>(move(upsert_request));

    upsert_database_item_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    ON_CALL(*connection_, Commit).WillByDefault(Return(CommitResult{}));
    ON_CALL(*spanner_factory_, CreateClients)
        .WillByDefault(Return(
            make_pair(make_shared<Client>(connection_),
                      make_shared<DatabaseAdminClient>(database_connection_))));

    EXPECT_SUCCESS(gcp_spanner_.Init());
    EXPECT_SUCCESS(gcp_spanner_.Run());
  }

  shared_ptr<MockInstanceClientProvider> instance_client_;
  shared_ptr<MockConnection> connection_;
  shared_ptr<MockDatabaseAdminConnection> database_connection_;
  shared_ptr<MockSpannerFactory> spanner_factory_;
  GcpSpannerClientProvider gcp_spanner_;

  AsyncContext<CreateTableRequest, CreateTableResponse> create_table_context_;

  AsyncContext<DeleteTableRequest, DeleteTableResponse> delete_table_context_;

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context_;

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  std::atomic_bool finish_called_{false};
};

TEST_F(GcpSpannerTests, InitWithGetProjectIdFailure) {
  auto instance_client = make_shared<NiceMock<MockInstanceClientProvider>>();
  instance_client->get_instance_resource_name_mock =
      FailureExecutionResult(123);
  GcpSpannerClientProvider gcp_spanner(
      make_shared<NoSQLDatabaseClientOptions>(NoSQLDatabaseClientOptions{
          "instance", "database", GetTableNameToKeysMap()}),
      instance_client, make_shared<MockAsyncExecutor>(),
      make_shared<MockAsyncExecutor>(), spanner_factory_);

  EXPECT_SUCCESS(gcp_spanner.Init());
  EXPECT_THAT(gcp_spanner.Run(), ResultIs(FailureExecutionResult(123)));
}

TEST_F(GcpSpannerTests, CreateTableNoSortKeySuccess) {
  create_table_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  create_table_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  create_table_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result, IsSuccessful());
    finish_called_ = true;
  };

  UpdateDatabaseDdlRequest expected_request;
  expected_request.set_database(
      "projects/123456789/instances/instance/databases/database");
  auto create_table_statement = absl::Substitute(
      R"(
      CREATE TABLE $0 (
        $1 STRING(MAX) NOT NULL,
        Value JSON
      ) PRIMARY KEY($1)
      )",
      kPartitionLockTableName, kPartitionLockPartitionKeyName);
  absl::RemoveExtraAsciiWhitespace(&create_table_statement);
  expected_request.add_statements(move(create_table_statement));
  EXPECT_CALL(*database_connection_,
              UpdateDatabaseDdl(EqualsProto(expected_request)))
      .WillOnce(ReturnEmptyUpdateMetadata);

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_), IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, CreateTableWithSortKeySuccess) {
  create_table_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  create_table_context_.request->mutable_key()->mutable_sort_key()->CopyFrom(
      MakeIntegerAttribute(kBudgetKeySortKeyName, 2));

  create_table_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result, IsSuccessful());
    finish_called_ = true;
  };

  UpdateDatabaseDdlRequest expected_request;
  expected_request.set_database(
      "projects/123456789/instances/instance/databases/database");
  auto create_table_statement = absl::Substitute(
      R"(
      CREATE TABLE $0 (
        $1 STRING(MAX) NOT NULL,
        $2 INT64 NOT NULL,
        Value JSON
      ) PRIMARY KEY($1, $2)
      )",
      kBudgetKeyTableName, kBudgetKeyPartitionKeyName, kBudgetKeySortKeyName);
  absl::RemoveExtraAsciiWhitespace(&create_table_statement);
  expected_request.add_statements(move(create_table_statement));
  EXPECT_CALL(*database_connection_,
              UpdateDatabaseDdl(EqualsProto(expected_request)))
      .WillOnce(ReturnEmptyUpdateMetadata);

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_), IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, CreateTableFailsNoTableName) {
  create_table_context_.request->mutable_key()->clear_table_name();

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_EMPTY_TABLE_NAME)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, CreateTableFailsBadPartitionKey) {
  create_table_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute("", "3"));

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME)));

  WaitUntil([this]() { return finish_called_.load(); });

  finish_called_ = false;
  create_table_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  create_table_context_.request->mutable_key()
      ->mutable_partition_key()
      ->clear_value();

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_UNSET_KEY_TYPE)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, CreateTableFailsBadSortKey) {
  create_table_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  create_table_context_.request->mutable_key()->mutable_sort_key()->CopyFrom(
      MakeStringAttribute("", "2"));

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME)));

  WaitUntil([this]() { return finish_called_.load(); });

  finish_called_ = false;
  create_table_context_.request->mutable_key()->mutable_sort_key()->CopyFrom(
      MakeStringAttribute(kBudgetKeySortKeyName, "2"));
  create_table_context_.request->mutable_key()
      ->mutable_sort_key()
      ->clear_value();

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_UNSET_KEY_TYPE)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, CreateTablePropagatesFailure) {
  create_table_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  create_table_context_.request->mutable_key()->mutable_sort_key()->CopyFrom(
      MakeIntegerAttribute(kBudgetKeySortKeyName, 2));

  create_table_context_.callback = [this](auto& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_GCP_INTERNAL_SERVICE_ERROR)));
    finish_called_ = true;
  };

  EXPECT_CALL(*database_connection_, UpdateDatabaseDdl)
      .WillOnce(
          Return(ByMove(make_ready_future(StatusOr<UpdateDatabaseDdlMetadata>(
              Status(google::cloud::StatusCode::kInternal, "Error"))))));

  EXPECT_THAT(gcp_spanner_.CreateTable(create_table_context_), IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, DeleteTableSuccess) {
  delete_table_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result, IsSuccessful());
    finish_called_ = true;
  };

  UpdateDatabaseDdlRequest expected_request;
  expected_request.set_database(
      "projects/123456789/instances/instance/databases/database");
  expected_request.add_statements(
      absl::StrCat("DROP TABLE ", kBudgetKeyTableName));
  EXPECT_CALL(*database_connection_,
              UpdateDatabaseDdl(EqualsProto(expected_request)))
      .WillOnce(ReturnEmptyUpdateMetadata);

  EXPECT_THAT(gcp_spanner_.DeleteTable(delete_table_context_), IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, DeleteTableFailsNoTableName) {
  delete_table_context_.request->clear_table_name();

  EXPECT_THAT(gcp_spanner_.DeleteTable(delete_table_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_EMPTY_TABLE_NAME)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, DeleteTablePropagatesFailure) {
  delete_table_context_.callback = [this](auto& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(SC_GCP_INTERNAL_SERVICE_ERROR)));
    finish_called_ = true;
  };

  EXPECT_CALL(*database_connection_, UpdateDatabaseDdl)
      .WillOnce(
          Return(ByMove(make_ready_future(StatusOr<UpdateDatabaseDdlMetadata>(
              Status(google::cloud::StatusCode::kInternal, "Error"))))));

  EXPECT_THAT(gcp_spanner_.DeleteTable(delete_table_context_), IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

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

MATCHER_P2(IsStringAttribute, name, value, "") {
  if (arg.value_case() != ItemAttribute::kValueString) {
    *result_listener << "Expected arg to have value_string: " << value
                     << " but has:\n"
                     << arg.DebugString();
    return false;
  }
  return ExplainMatchResult(Eq(value), arg.value_string(), result_listener) &&
         ExplainMatchResult(Eq(name), arg.name(), result_listener);
}

TEST_F(GcpSpannerTests, GetItemWithPartitionKeyOnly) {
  get_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  get_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM `PartitionLock` WHERE LockId = "
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
    EXPECT_EQ(response->item().key().table_name(), kPartitionLockTableName);
    EXPECT_THAT(response->item().key().partition_key(),
                IsStringAttribute(kPartitionLockPartitionKeyName, "3"));
    EXPECT_FALSE(response->item().key().has_sort_key());
    EXPECT_THAT(response->item().attributes(),
                UnorderedElementsAre(IsStringAttribute("token_count", "1")));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemWithPartitionAndSortKey) {
  get_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));

  get_database_item_context_.request->mutable_key()
      ->mutable_sort_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeySortKeyName, "2"));

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM `BudgetKeys` WHERE BudgetKeyId = "
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
    EXPECT_EQ(response->item().key().table_name(), kBudgetKeyTableName);
    EXPECT_THAT(response->item().key().partition_key(),
                IsStringAttribute(kBudgetKeyPartitionKeyName, "3"));
    EXPECT_THAT(response->item().key().sort_key(),
                IsStringAttribute(kBudgetKeySortKeyName, "2"));
    EXPECT_THAT(response->item().attributes(),
                UnorderedElementsAre(IsStringAttribute("token_count", "1")));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemWithPartitionAndSortKeyWithAttributes) {
  get_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));

  get_database_item_context_.request->mutable_key()
      ->mutable_sort_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeySortKeyName, "2"));
  get_database_item_context_.request->add_required_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM `BudgetKeys` WHERE BudgetKeyId = "
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
    EXPECT_EQ(response->item().key().table_name(), kBudgetKeyTableName);
    EXPECT_THAT(response->item().key().partition_key(),
                IsStringAttribute(kBudgetKeyPartitionKeyName, "3"));
    EXPECT_THAT(response->item().key().sort_key(),
                IsStringAttribute(kBudgetKeySortKeyName, "2"));
    EXPECT_THAT(response->item().attributes(),
                UnorderedElementsAre(IsStringAttribute("token_count", "1")));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemNoRowFound) {
  get_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  get_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM `PartitionLock` WHERE LockId = "
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
                ResultIs(FailureExecutionResult(
                    SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)));
    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemJsonParseFail) {
  get_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  get_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  auto expected_query =
      "SELECT IFNULL(Value, JSON '{}') FROM `PartitionLock` WHERE LockId = "
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
                ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, GetItemFailsIfBadPartitionKey) {
  get_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute("some_other_key", "3"));

  auto result = gcp_spanner_.GetDatabaseItem(get_database_item_context_);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME)));

  EXPECT_TRUE(finish_called_);
}

TEST_F(GcpSpannerTests, GetItemFailsIfBadSortKey) {
  get_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  // Sort key is bad because it's absent.

  EXPECT_THAT(gcp_spanner_.GetDatabaseItem(get_database_item_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME)));

  EXPECT_TRUE(finish_called_);
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesWithPartitionKeyOnly) {
  upsert_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  SqlStatement expected_sql(
      "SELECT Value FROM `PartitionLock` WHERE LockId = @partition_key",
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

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesWithSortKey) {
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  upsert_database_item_context_.request->mutable_key()
      ->mutable_sort_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeySortKeyName, "2"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  SqlStatement expected_sql(
      "SELECT Value FROM `BudgetKeys` WHERE BudgetKeyId = @partition_key AND "
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

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesWithExistingValue) {
  upsert_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  SqlStatement expected_sql(
      "SELECT Value FROM `PartitionLock` WHERE LockId = @partition_key",
      move(params));
  auto returned_results = make_unique<MockResultSetSource>();
  // We return a JSON with "other_val" and "token_count" existing,
  // token_count should be overridden.
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

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesFailsIfCommitFails) {
  upsert_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

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
                ResultIs(RetryExecutionResult(
                    SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemNoAttributesFailsIfJsonParseFails) {
  upsert_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

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
                ResultIs(FailureExecutionResult(
                    SC_NO_SQL_DATABASE_PROVIDER_RECORD_CORRUPTED)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesWithPartitionKeyOnly) {
  upsert_database_item_context_.request->mutable_key()->set_table_name(
      kPartitionLockTableName);
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kPartitionLockPartitionKeyName, "3"));

  upsert_database_item_context_.request->add_required_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "100"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM `PartitionLock` WHERE LockId = @partition_key AND "
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

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesWithSortKey) {
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  upsert_database_item_context_.request->mutable_key()
      ->mutable_sort_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeySortKeyName, "2"));

  upsert_database_item_context_.request->add_required_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "100"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM `BudgetKeys` WHERE BudgetKeyId = @partition_key AND "
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

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesFailsIfNoRowsFound) {
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  upsert_database_item_context_.request->mutable_key()
      ->mutable_sort_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeySortKeyName, "2"));

  upsert_database_item_context_.request->add_required_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "100"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM `BudgetKeys` WHERE BudgetKeyId = @partition_key AND "
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
                ResultIs(FailureExecutionResult(
                    SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemWithAttributesFailsIfCommitFails) {
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  upsert_database_item_context_.request->mutable_key()
      ->mutable_sort_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeySortKeyName, "2"));

  upsert_database_item_context_.request->add_required_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "100"));

  upsert_database_item_context_.request->add_new_attributes()->CopyFrom(
      MakeStringAttribute("token_count", "1"));

  SqlStatement::ParamType params;
  params.emplace("partition_key", "3");
  params.emplace("sort_key", "2");
  params.emplace("attribute_0", "100");
  SqlStatement expected_sql(
      "SELECT Value FROM `BudgetKeys` WHERE BudgetKeyId = @partition_key AND "
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
                ResultIs(RetryExecutionResult(
                    SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)));

    finish_called_ = true;
  };

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpSpannerTests, UpsertItemFailsIfBadPartitionKey) {
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute("some_other_key", "3"));

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME)));

  EXPECT_TRUE(finish_called_);
}

TEST_F(GcpSpannerTests, UpsertItemFailsIfBadSortKey) {
  upsert_database_item_context_.request->mutable_key()
      ->mutable_partition_key()
      ->CopyFrom(MakeStringAttribute(kBudgetKeyPartitionKeyName, "3"));
  // Sort key is bad because it's absent.

  EXPECT_THAT(gcp_spanner_.UpsertDatabaseItem(upsert_database_item_context_),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME)));

  EXPECT_TRUE(finish_called_);
}

}  // namespace google::scp::cpio::client_providers::test
