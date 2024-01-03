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

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/logger_interface.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "core/nosql_database_provider/src/gcp/gcp_spanner.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/value.h"
#include "public/core/test/interface/execution_result_matchers.h"

namespace google::scp::core::test {
namespace {

using google::cloud::StatusOr;
using google::cloud::spanner::Client;
using google::cloud::spanner::Database;
using google::cloud::spanner::Json;
using google::cloud::spanner::KeySet;
using google::cloud::spanner::MakeKey;
using google::cloud::spanner::MakeNullValue;
using google::cloud::spanner::Mutations;
using google::cloud::spanner::Row;
using google::cloud::spanner::RowStream;
using google::cloud::spanner::SqlStatement;
using google::cloud::spanner::StreamOf;
using google::cloud::spanner::Value;
using google::cloud::spanner_admin::DatabaseAdminClient;
using google::cloud::spanner_admin::MakeDatabaseAdminConnection;
using google::cloud::spanner_mocks::MakeRow;
using google::cloud::spanner_mocks::MockConnection;
using google::cloud::spanner_mocks::MockResultSetSource;
using google::scp::core::AsyncExecutor;
using google::scp::core::LoggerInterface;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::GlobalLogger;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::Logger;
using google::scp::core::nosql_database_provider::GcpSpanner;
using google::scp::core::test::TestLoggingUtils;
using std::get;
using std::make_shared;
using std::make_tuple;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::thread;
using std::tuple;
using std::unique_ptr;
using std::vector;
using testing::_;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::FieldsAre;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::Pointee;
using testing::PrintToString;
using testing::Return;
using testing::StrEq;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
using testing::VariantWith;

constexpr char kTableName[] = "BudgetKeys";
constexpr char kDefaultPartitionKeyName[] = "BudgetKeyId";
constexpr char kDefaultSortKeyName[] = "Timeframe";
constexpr char kValueName[] = "Value";

#define PROJECT "not-a-real-project"
#define INSTANCE "test-instance"
#define DATABASE "test-database"

constexpr char kProject[] = PROJECT;
constexpr char kInstance[] = INSTANCE;
constexpr char kProjectInstance[] = "projects/" PROJECT "/instances/" INSTANCE;

constexpr char kDatabase[] = DATABASE;
constexpr char kDatabaseUri[] =
    "projects/" PROJECT "/instances/" INSTANCE "/databases/" DATABASE;

constexpr size_t kThreadCount = 5;
constexpr size_t kQueueSize = 100;

class GcpSpannerAsyncTests : public testing::TestWithParam<int> {
 protected:
  static void SetUpTestSuite() {
    TestLoggingUtils::EnableLogOutputToConsole();
    database_admin_client_ =
        new DatabaseAdminClient(MakeDatabaseAdminConnection());
    client_ =
        new Client(MakeConnection(Database(kProject, kInstance, kDatabase)));
    SetUpDatabase();
  }

  static void SetUpDatabase() {
    auto db =
        database_admin_client_
            ->CreateDatabase(kProjectInstance,
                             absl::StrCat("CREATE DATABASE `", kDatabase, "`"))
            .get();
    ASSERT_TRUE(db.ok()) << db.status().message();

    std::vector<std::string> statements;
    statements.push_back(absl::Substitute(R"""(
        CREATE TABLE $0 (
          $1 STRING(MAX) NOT NULL,
          $2 STRING(MAX),
          Value JSON
        ) PRIMARY KEY($1, $2)
      )""",
                                          kTableName, kDefaultPartitionKeyName,
                                          kDefaultSortKeyName));
    auto update_ddl_result =
        database_admin_client_->UpdateDatabaseDdl(kDatabaseUri, statements)
            .get();
    ASSERT_TRUE(update_ddl_result.ok()) << update_ddl_result.status().message();
  }

  static void ClearTableAndInsertDefaultRows() {
    auto commit_result = client_->Commit([](auto txn) -> StatusOr<Mutations> {
      auto clear = client_->ExecuteDml(
          txn, SqlStatement(
                   absl::StrFormat("DELETE FROM %s WHERE TRUE", kTableName)));
      if (!clear) return move(clear).status();
      auto insert = client_->ExecuteDml(
          move(txn),
          SqlStatement(absl::Substitute(R"""(
                INSERT INTO $0 ($1, $2, $3)
                VALUES
                  ('1', '2', JSON '{"token_count": "10"}'),
                  ('2', '2', JSON '{"token_count": "20"}'),
                  ('3', '2', JSON '{"token_count": "30"}')
              )""",
                                        kTableName, kDefaultPartitionKeyName,
                                        kDefaultSortKeyName, kValueName)));
      if (!insert) return move(insert).status();
      return Mutations{};
    });
    ASSERT_TRUE(commit_result.ok()) << commit_result.status().message();
  }

  static void ClearTableAndInsertRowWithCount(int token_count) {
    auto commit_result =
        client_->Commit([token_count](auto txn) -> StatusOr<Mutations> {
          auto clear = client_->ExecuteDml(
              txn, SqlStatement(absl::StrFormat("DELETE FROM %s WHERE TRUE",
                                                kTableName)));
          if (!clear) return move(clear).status();
          auto insert = client_->ExecuteDml(
              move(txn), SqlStatement(absl::Substitute(
                             R"""(
                INSERT INTO $0 ($1, $2, $3)
                VALUES
                  ('1', '2', JSON '{"token_count": "$4"}')
              )""",
                             kTableName, kDefaultPartitionKeyName,
                             kDefaultSortKeyName, kValueName, token_count)));
          if (!insert) return move(insert).status();
          return Mutations{};
        });
    ASSERT_TRUE(commit_result.ok()) << commit_result.status().message();
  }

  static void TearDownTestSuite() {
    auto status = database_admin_client_->DropDatabase(kDatabaseUri);
    delete database_admin_client_;
    delete client_;
  }

  GcpSpannerAsyncTests()
      : async_executor_(make_shared<AsyncExecutor>(kThreadCount, kQueueSize)),
        io_async_executor_(
            make_shared<AsyncExecutor>(kThreadCount, kQueueSize)),
        config_provider_(make_shared<MockConfigProvider>()) {
    ClearTableAndInsertDefaultRows();
    NoSqlDatabaseKeyValuePair partition_key{
        make_shared<NoSQLDatabaseAttributeName>(),
        make_shared<NoSQLDatabaseValidAttributeValueTypes>()};
    NoSqlDatabaseKeyValuePair sort_key{
        make_shared<NoSQLDatabaseAttributeName>(),
        make_shared<NoSQLDatabaseValidAttributeValueTypes>()};

    GetDatabaseItemRequest get_request;
    get_request.table_name = make_shared<string>(kTableName);
    get_request.partition_key =
        make_shared<NoSqlDatabaseKeyValuePair>(partition_key);
    get_request.sort_key = make_shared<NoSqlDatabaseKeyValuePair>(sort_key);

    get_database_item_context_.request =
        make_shared<GetDatabaseItemRequest>(move(get_request));

    UpsertDatabaseItemRequest upsert_request;
    upsert_request.table_name = make_shared<string>(kTableName);
    upsert_request.partition_key =
        make_shared<NoSqlDatabaseKeyValuePair>(partition_key);
    upsert_request.sort_key = make_shared<NoSqlDatabaseKeyValuePair>(sort_key);

    upsert_database_item_context_.request =
        make_shared<UpsertDatabaseItemRequest>(move(upsert_request));

    config_provider_->Set(kGcpProjectId, string(kProject));
    config_provider_->Set(kSpannerInstance, string(kInstance));
    config_provider_->Set(kSpannerDatabase, string(kDatabase));

    async_executor_->Init();
    async_executor_->Run();
    io_async_executor_->Init();
    io_async_executor_->Run();

    gcp_spanner_ = make_unique<GcpSpanner>(
        async_executor_, io_async_executor_, config_provider_,
        nullptr /* table_name_to_keys */, AsyncPriority::Normal,
        AsyncPriority::Normal);
    gcp_spanner_->Init();
    gcp_spanner_->Run();
  }

  ~GcpSpannerAsyncTests() {
    async_executor_->Stop();
    io_async_executor_->Stop();
    gcp_spanner_->Stop();
  }

  int GetThreadCount() { return GetParam(); }

  void ExpectTableHasRows(
      const vector<tuple<Value, Value, Value>>& expected_rows) {
    auto row_stream = client_->Read(
        kTableName, KeySet::All(),
        {kDefaultPartitionKeyName, kDefaultSortKeyName, kValueName});

    vector<tuple<Value, Value, Value>> actual_rows;
    for (const auto& row : row_stream) {
      if (!row.ok()) break;
      Value budget_key_id, timeframe, value;
      if (auto status_or = row->get(kDefaultPartitionKeyName); status_or.ok()) {
        budget_key_id = move(*status_or);
      }
      if (auto status_or = row->get(kDefaultSortKeyName); status_or.ok()) {
        timeframe = move(*status_or);
      }
      if (auto status_or = row->get(kValueName); status_or.ok()) {
        value = move(*status_or);
      }
      actual_rows.push_back(
          make_tuple(move(budget_key_id), move(timeframe), move(value)));
    }
    EXPECT_THAT(actual_rows, UnorderedElementsAreArray(expected_rows));
  }

  shared_ptr<AsyncExecutor> async_executor_, io_async_executor_;
  shared_ptr<MockConfigProvider> config_provider_;
  unique_ptr<GcpSpanner> gcp_spanner_;

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context_;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context_;

  static DatabaseAdminClient* database_admin_client_;
  static Client* client_;
};

DatabaseAdminClient* GcpSpannerAsyncTests::database_admin_client_;
Client* GcpSpannerAsyncTests::client_;

TEST_F(GcpSpannerAsyncTests, SimpleGetTest) {
  std::atomic_bool finished(false);
  *get_database_item_context_.request->partition_key->attribute_name =
      kDefaultPartitionKeyName;
  *get_database_item_context_.request->partition_key->attribute_value = "1";
  *get_database_item_context_.request->sort_key->attribute_name =
      kDefaultSortKeyName;
  *get_database_item_context_.request->sort_key->attribute_value = "2";

  get_database_item_context_.callback = [&finished](auto& context) {
    const auto& response = context.response;
    ASSERT_THAT(response, NotNull());
    EXPECT_THAT(response->partition_key,
                Pointee(FieldsAre(Pointee(StrEq(kDefaultPartitionKeyName)),
                                  Pointee(VariantWith<string>("1")))));
    EXPECT_THAT(response->sort_key,
                Pointee(FieldsAre(Pointee(StrEq(kDefaultSortKeyName)),
                                  Pointee(VariantWith<string>("2")))));
    EXPECT_THAT(response->attributes,
                Pointee(UnorderedElementsAre(
                    FieldsAre(Pointee(StrEq("token_count")),
                              Pointee(VariantWith<string>("10"))))));
    finished = true;
  };

  EXPECT_SUCCESS(gcp_spanner_->GetDatabaseItem(get_database_item_context_));

  WaitUntil([&finished]() -> bool { return finished; });
}

std::atomic_int failure_count{0};

TEST_P(GcpSpannerAsyncTests, MultiThreadGetTest) {
  // Vector of signals for when a thread has finished - index aligned with
  // threads.
  vector<std::atomic_bool> finished(GetThreadCount());
  for (auto& b : finished) b = false;

  std::atomic_bool start{false};
  vector<thread> threads;
  threads.reserve(GetThreadCount());
  for (int i = 0; i < GetThreadCount(); i++) {
    threads.push_back(thread([this, &start, i = i, &finished] {
      WaitUntil([&start]() -> bool { return start; });

      string partition_key_val = std::to_string((i % 3) + 1);
      string token_count_val = std::to_string(((i % 3) + 1) * 10);

      NoSqlDatabaseKeyValuePair partition_key{
          make_shared<NoSQLDatabaseAttributeName>(),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>()};
      *partition_key.attribute_name = kDefaultPartitionKeyName;
      *partition_key.attribute_value = partition_key_val;

      NoSqlDatabaseKeyValuePair sort_key{
          make_shared<NoSQLDatabaseAttributeName>(),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>()};
      *sort_key.attribute_name = kDefaultSortKeyName;
      *sort_key.attribute_value = "2";

      GetDatabaseItemRequest request;
      request.table_name = make_shared<string>(kTableName);
      request.partition_key =
          make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
      request.sort_key = make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));

      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
          get_database_item_context;
      get_database_item_context.request =
          make_shared<GetDatabaseItemRequest>(move(request));

      ExecutionResult result;
      get_database_item_context.callback = [&result, &partition_key_val,
                                            &token_count_val, &finished,
                                            i](auto& context) {
        result = context.result;
        if (result ==
            FailureExecutionResult(
                errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)) {
          return;
        }

        const auto& response = context.response;
        ASSERT_THAT(response, NotNull());
        EXPECT_THAT(response->partition_key,
                    Pointee(FieldsAre(
                        Pointee(StrEq(kDefaultPartitionKeyName)),
                        Pointee(VariantWith<string>(partition_key_val)))));
        EXPECT_THAT(response->attributes,
                    Pointee(UnorderedElementsAre(FieldsAre(
                        Pointee(StrEq("token_count")),
                        Pointee(VariantWith<string>(token_count_val))))));
        finished[i] = true;
      };

      EXPECT_SUCCESS(gcp_spanner_->GetDatabaseItem(get_database_item_context));

      WaitUntil([&finished, i]() -> bool { return finished[i]; });

      if (result == FailureExecutionResult(
                        errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)) {
        failure_count++;
        return;
      }
    }));
  }
  start = true;
  for (auto& t : threads) t.join();

  if (failure_count > 1) {
    FAIL() << "Only one failure is expected with the emulator.";
  }
}

TEST_F(GcpSpannerAsyncTests, SimpleUpsertTestNoAttributes) {
  std::atomic_bool finished(false);
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kDefaultPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "1";
  *upsert_database_item_context_.request->sort_key->attribute_name =
      kDefaultSortKeyName;
  *upsert_database_item_context_.request->sort_key->attribute_value = "2";

  upsert_database_item_context_.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1000")});

  upsert_database_item_context_.callback = [&finished](auto& context) {
    EXPECT_SUCCESS(context.result);

    finished = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_->UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([&finished]() -> bool { return finished; });

  // Test.

  vector<tuple<Value, Value, Value>> expected_rows;
  expected_rows.push_back(make_tuple(
      Value("1"), Value("2"), Value(Json("{\"token_count\":\"1000\"}"))));
  expected_rows.push_back(make_tuple(Value("2"), Value("2"),
                                     Value(Json("{\"token_count\":\"20\"}"))));
  expected_rows.push_back(make_tuple(Value("3"), Value("2"),
                                     Value(Json("{\"token_count\":\"30\"}"))));
  ExpectTableHasRows(expected_rows);
}

TEST_F(GcpSpannerAsyncTests, SimpleUpsertTestWithSortKeyNoAttributes) {
  std::atomic_bool finished(false);
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kDefaultPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "1";
  *upsert_database_item_context_.request->sort_key->attribute_name =
      kDefaultSortKeyName;
  *upsert_database_item_context_.request->sort_key->attribute_value = "0";

  upsert_database_item_context_.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1000")});

  upsert_database_item_context_.callback = [&finished](auto& context) {
    EXPECT_SUCCESS(context.result);

    finished = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_->UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([&finished]() -> bool { return finished; });

  // Test.
  vector<tuple<Value, Value, Value>> expected_rows;
  expected_rows.push_back(make_tuple(
      Value("1"), Value("0"), Value(Json("{\"token_count\":\"1000\"}"))));
  expected_rows.push_back(make_tuple(Value("1"), Value("2"),
                                     Value(Json("{\"token_count\":\"10\"}"))));
  expected_rows.push_back(make_tuple(Value("2"), Value("2"),
                                     Value(Json("{\"token_count\":\"20\"}"))));
  expected_rows.push_back(make_tuple(Value("3"), Value("2"),
                                     Value(Json("{\"token_count\":\"30\"}"))));
  ExpectTableHasRows(expected_rows);
}

TEST_F(GcpSpannerAsyncTests, SimpleUpsertTestWithSortKeyAndAttributes) {
  std::atomic_bool finished(false);
  *upsert_database_item_context_.request->partition_key->attribute_name =
      kDefaultPartitionKeyName;
  *upsert_database_item_context_.request->partition_key->attribute_value = "1";
  *upsert_database_item_context_.request->sort_key->attribute_name =
      kDefaultSortKeyName;
  *upsert_database_item_context_.request->sort_key->attribute_value = "2";

  upsert_database_item_context_.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("10")});

  upsert_database_item_context_.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context_.request->new_attributes->push_back(
      NoSqlDatabaseKeyValuePair{
          .attribute_name = make_shared<string>("token_count"),
          .attribute_value =
              make_shared<NoSQLDatabaseValidAttributeValueTypes>("1000")});

  upsert_database_item_context_.callback = [&finished](auto& context) {
    EXPECT_SUCCESS(context.result);

    finished = true;
  };

  EXPECT_SUCCESS(
      gcp_spanner_->UpsertDatabaseItem(upsert_database_item_context_));

  WaitUntil([&finished]() -> bool { return finished; });

  // Test.
  vector<tuple<Value, Value, Value>> expected_rows;
  expected_rows.push_back(make_tuple(
      Value("1"), Value("2"), Value(Json("{\"token_count\":\"1000\"}"))));
  expected_rows.push_back(make_tuple(Value("2"), Value("2"),
                                     Value(Json("{\"token_count\":\"20\"}"))));
  expected_rows.push_back(make_tuple(Value("3"), Value("2"),
                                     Value(Json("{\"token_count\":\"30\"}"))));
  ExpectTableHasRows(expected_rows);
}

TEST_P(GcpSpannerAsyncTests, MultiThreadUpsertTest) {
  std::atomic_bool start(false);
  // Vector of signals for when a thread has finished - index aligned with
  // threads.
  vector<std::atomic_bool> finished(GetThreadCount());
  for (auto& b : finished) b = false;

  vector<thread> threads;
  threads.reserve(GetThreadCount());
  for (int i = 0; i < GetThreadCount(); i++) {
    threads.push_back(thread([this, &start, i = i, &finished]() {
      WaitUntil([&start]() -> bool { return start; });

      string partition_key_str = absl::StrCat(i + 1);
      string sort_key_str = "2";
      string token_count = absl::StrCat(1000 * (i + 1));

      NoSqlDatabaseKeyValuePair partition_key{
          make_shared<string>(kDefaultPartitionKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(
              partition_key_str)};

      NoSqlDatabaseKeyValuePair sort_key{
          make_shared<string>(kDefaultSortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(sort_key_str)};

      UpsertDatabaseItemRequest upsert_request;
      upsert_request.table_name = make_shared<string>(kTableName);
      upsert_request.partition_key =
          make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
      upsert_request.sort_key =
          make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));

      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
          upsert_database_item_context;

      upsert_database_item_context.request =
          make_shared<UpsertDatabaseItemRequest>(move(upsert_request));

      upsert_database_item_context.request->new_attributes =
          make_shared<vector<NoSqlDatabaseKeyValuePair>>();
      upsert_database_item_context.request->new_attributes->push_back(
          NoSqlDatabaseKeyValuePair{
              .attribute_name = make_shared<string>("token_count"),
              .attribute_value =
                  make_shared<NoSQLDatabaseValidAttributeValueTypes>(
                      token_count)});

      upsert_database_item_context.callback = [&finished, i](auto& context) {
        EXPECT_SUCCESS(context.result);

        finished[i] = true;
      };

      EXPECT_SUCCESS(
          gcp_spanner_->UpsertDatabaseItem(upsert_database_item_context));

      WaitUntil([&finished, i]() -> bool { return finished[i]; });
    }));
  }
  start = true;
  for (auto& t : threads) t.join();

  vector<tuple<Value, Value, Value>> expected_rows;
  for (int i = 0; i < GetThreadCount(); i++) {
    expected_rows.push_back(
        make_tuple(Value(absl::StrCat(i + 1)), Value("2"),
                   Value(Json(absl::StrFormat("{\"token_count\":\"%d\"}",
                                              1000 * (i + 1))))));
  }
  ExpectTableHasRows(expected_rows);
}

// Enter a row with ("1", "2", '{ "token_count": "thread_count * 10"')
// Spawn a number of threads, have each thread 1. fetch the current token_count
// then 2. decrement that count by 1 a total of 10 times.
// Check that the row has 0 token count.
TEST_P(GcpSpannerAsyncTests, MultiThreadGetAndConditionalUpsertTest) {
  ClearTableAndInsertRowWithCount(GetThreadCount() * 10);

  std::atomic_bool start(false);
  std::atomic_int total_rpc_count(0);

  vector<thread> threads;
  threads.reserve(GetThreadCount());
  for (int i = 0; i < GetThreadCount(); i++) {
    threads.push_back(thread([this, &start, &total_rpc_count]() {
      WaitUntil([&start]() -> bool { return start; });

      std::atomic_bool finished(false);

      NoSqlDatabaseKeyValuePair partition_key{
          make_shared<string>(kDefaultPartitionKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("1")};

      NoSqlDatabaseKeyValuePair sort_key{
          make_shared<string>(kDefaultSortKeyName),
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("2")};

      for (int budget_consumption_count = 10; budget_consumption_count > 0;) {
        // Reset finished indicator for another consume command.
        finished = false;
        // Get the current token_count.
        GetDatabaseItemRequest get_request;
        get_request.table_name = make_shared<string>(kTableName);
        get_request.partition_key =
            make_shared<NoSqlDatabaseKeyValuePair>(partition_key);
        get_request.sort_key = make_shared<NoSqlDatabaseKeyValuePair>(sort_key);

        AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
            get_database_item_context;

        get_database_item_context.request =
            make_shared<GetDatabaseItemRequest>(move(get_request));

        shared_ptr<GetDatabaseItemResponse> response;
        get_database_item_context.callback = [&response,
                                              &finished](auto& context) {
          ASSERT_THAT(context.result, IsSuccessful());
          response = context.response;

          finished = true;
        };

        ASSERT_THAT(gcp_spanner_->GetDatabaseItem(get_database_item_context),
                    IsSuccessful());
        WaitUntil([&finished]() -> bool { return finished; });

        ///////////////////////////////////////////////////////////////////////
        finished = false;

        // Decrement the received count by 1.
        auto& existing_count = response->attributes->at(0).attribute_value;
        // Convert the variant to a string then to an int and subtract 1, then
        // stringify again.
        string new_count =
            absl::StrCat(std::stoi(std::get<string>(*existing_count)) - 1);

        UpsertDatabaseItemRequest upsert_request;
        upsert_request.table_name = make_shared<string>(kTableName);
        upsert_request.partition_key =
            make_shared<NoSqlDatabaseKeyValuePair>(partition_key);
        upsert_request.sort_key =
            make_shared<NoSqlDatabaseKeyValuePair>(sort_key);

        upsert_request.attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>();
        upsert_request.attributes->push_back(NoSqlDatabaseKeyValuePair{
            .attribute_name = make_shared<string>("token_count"),
            .attribute_value = move(existing_count)});

        upsert_request.new_attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>();
        upsert_request.new_attributes->push_back(NoSqlDatabaseKeyValuePair{
            .attribute_name = make_shared<string>("token_count"),
            .attribute_value =
                make_shared<NoSQLDatabaseValidAttributeValueTypes>(new_count)});

        AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
            upsert_database_item_context;

        upsert_database_item_context.request =
            make_shared<UpsertDatabaseItemRequest>(move(upsert_request));

        upsert_database_item_context.callback = [&budget_consumption_count,
                                                 &total_rpc_count,
                                                 &finished](auto& context) {
          if (context.result.Successful()) {
            budget_consumption_count--;
          }
          // Ignore any error here.
          total_rpc_count += 3;
          finished = true;
        };

        ASSERT_THAT(
            gcp_spanner_->UpsertDatabaseItem(upsert_database_item_context),
            IsSuccessful());

        WaitUntil([&finished]() -> bool { return finished; });
      }
    }));
  }
  start = true;
  for (auto& t : threads) t.join();

  vector<tuple<Value, Value, Value>> expected_rows;
  expected_rows.push_back(make_tuple(Value("1"), Value("2"),
                                     Value(Json("{\"token_count\":\"0\"}"))));
  ExpectTableHasRows(expected_rows);

  std::cerr << "Took " << total_rpc_count.load() << " RPCs to complete for "
            << GetParam() << " threads" << std::endl;
}

INSTANTIATE_TEST_SUITE_P(MultiThreadedTests, GcpSpannerAsyncTests,
                         testing::Values(3, 5, 10, 20, 50));

}  // namespace
}  // namespace google::scp::core::test
