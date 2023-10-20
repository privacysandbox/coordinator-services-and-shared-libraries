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

#include <atomic>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/dynamodb/DynamoDBErrors.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/lease_manager/src/lease_manager.h"
#include "core/nosql_database_provider/mock/aws/mock_aws_dynamo_db.h"
#include "core/nosql_database_provider/mock/aws/mock_aws_dynamo_db_client.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/leasable_lock/src/leasable_lock_on_nosql_database.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::Map;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::String;
using Aws::Client::AsyncCallerContext;
using Aws::Client::AWSError;
using Aws::DynamoDB::DynamoDBClient;
using Aws::DynamoDB::DynamoDBErrors;
using Aws::DynamoDB::GetItemResponseReceivedHandler;
using Aws::DynamoDB::QueryResponseReceivedHandler;
using Aws::DynamoDB::UpdateItemResponseReceivedHandler;
using Aws::DynamoDB::Model::AttributeValue;
using Aws::DynamoDB::Model::GetItemRequest;
using Aws::DynamoDB::Model::QueryOutcome;
using Aws::DynamoDB::Model::QueryRequest;
using Aws::DynamoDB::Model::QueryResult;
using Aws::DynamoDB::Model::UpdateItemOutcome;
using Aws::DynamoDB::Model::UpdateItemRequest;
using Aws::DynamoDB::Model::UpdateItemResult;
using google::scp::core::AsyncExecutor;
using google::scp::core::LeaseManager;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::ConcurrentMap;
using google::scp::core::nosql_database_provider::aws::mock::MockAwsDynamoDB;
using google::scp::core::nosql_database_provider::aws::mock::
    MockAwsDynamoDBClient;
using google::scp::core::test::WaitUntil;
using google::scp::pbs::LeasableLockOnNoSQLDatabase;
using std::atomic;
using std::get;
using std::isxdigit;
using std::make_pair;
using std::make_shared;
using std::move;
using std::mt19937;
using std::random_device;
using std::shared_ptr;
using std::string;
using std::uniform_int_distribution;
using std::vector;

namespace google::scp::core::test {
TEST(AwsDynamoDBTests, GetItemWithPartitionAndSortKeyWithoutAttributes) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  get_database_item_context.request = make_shared<GetDatabaseItemRequest>();
  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));

  dynamo_db_client->query_async_mock =
      [&](const QueryRequest& query_request,
          const QueryResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(string(query_request.GetTableName().c_str()),
                  *get_database_item_context.request->table_name);
        EXPECT_EQ(string(query_request.GetKeyConditionExpression().c_str()),
                  "Col1= :partition_key and Col2= :sort_key");
        auto attributes = query_request.GetExpressionAttributeValues();
        EXPECT_EQ(attributes[String(":partition_key")].GetN(), "3");
        EXPECT_EQ(attributes[String(":sort_key")].GetN(), "2");
        EXPECT_EQ(query_request.GetFilterExpression().length(), 0);
      };

  dynamo_db.GetDatabaseItem(get_database_item_context);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, GetItemWithPartitionAndSortKeyWithAttributes) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  get_database_item_context.request = make_shared<GetDatabaseItemRequest>();
  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  get_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();

  NoSqlDatabaseKeyValuePair attribute_1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr1"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("1234")};

  NoSqlDatabaseKeyValuePair attribute_2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute_3{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr3"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(4.5)};

  get_database_item_context.request->attributes->push_back(attribute_1);
  get_database_item_context.request->attributes->push_back(attribute_2);
  get_database_item_context.request->attributes->push_back(attribute_3);

  dynamo_db_client->query_async_mock =
      [&](const QueryRequest& query_request,
          const QueryResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(string(query_request.GetTableName().c_str()),
                  *get_database_item_context.request->table_name);
        EXPECT_EQ(string(query_request.GetKeyConditionExpression().c_str()),
                  "Col1= :partition_key and Col2= :sort_key");
        EXPECT_EQ(query_request.GetFilterExpression(),
                  "Attr1= :attribute_0 and Attr2= :attribute_1 and Attr3= "
                  ":attribute_2");

        auto attributes = query_request.GetExpressionAttributeValues();
        EXPECT_EQ(attributes[String(":partition_key")].GetN(), "3");
        EXPECT_EQ(attributes[String(":sort_key")].GetN(), "2");
        EXPECT_EQ(attributes[String(":attribute_0")].GetS(), "1234");
        EXPECT_EQ(attributes[String(":attribute_1")].GetN(), "2");
        EXPECT_EQ(attributes[String(":attribute_2")].GetN(), "4.500000");
      };

  dynamo_db.GetDatabaseItem(get_database_item_context);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, OnGetDatabaseItemCallbackFailure) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  get_database_item_context.request = make_shared<GetDatabaseItemRequest>();
  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  get_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();

  NoSqlDatabaseKeyValuePair attribute_1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr1"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("1234")};

  NoSqlDatabaseKeyValuePair attribute_2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute_3{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr3"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(4.5)};

  get_database_item_context.request->attributes->push_back(attribute_1);
  get_database_item_context.request->attributes->push_back(attribute_2);
  get_database_item_context.request->attributes->push_back(attribute_3);
  get_database_item_context.callback =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              get_database_item_context) {
        EXPECT_THAT(get_database_item_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR)));
      };

  QueryRequest query_request;
  QueryRequest query_result;
  AWSError<DynamoDBErrors> dynamo_db_error(DynamoDBErrors::BACKUP_IN_USE,
                                           false);

  QueryOutcome outcome(dynamo_db_error);

  dynamo_db.OnGetDatabaseItemCallback(get_database_item_context, NULL,
                                      query_request, outcome, nullptr);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, OnGetDatabaseItemCallbackZeroOrMoreThanOneResult) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  get_database_item_context.request = make_shared<GetDatabaseItemRequest>();
  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  get_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();

  NoSqlDatabaseKeyValuePair attribute_1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr1"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("1234")};

  NoSqlDatabaseKeyValuePair attribute_2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute_3{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr3"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(4.5)};

  get_database_item_context.request->attributes->push_back(attribute_1);
  get_database_item_context.request->attributes->push_back(attribute_2);
  get_database_item_context.request->attributes->push_back(attribute_3);
  get_database_item_context.callback =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              get_database_item_context) {
        EXPECT_THAT(get_database_item_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)));
      };

  QueryRequest query_request;
  QueryResult query_result;
  QueryOutcome outcome(query_result);

  dynamo_db.OnGetDatabaseItemCallback(get_database_item_context, NULL,
                                      query_request, outcome, nullptr);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, OnGetDatabaseItemCallbackOneResult) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  get_database_item_context.request = make_shared<GetDatabaseItemRequest>();
  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  get_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();

  get_database_item_context
      .callback = [&](AsyncContext<GetDatabaseItemRequest,
                                   GetDatabaseItemResponse>&
                          get_database_item_context) {
    EXPECT_SUCCESS(get_database_item_context.result);
    EXPECT_EQ(
        *get_database_item_context.response->partition_key->attribute_name,
        *get_database_item_context.request->partition_key->attribute_name);
    EXPECT_EQ(
        *get_database_item_context.response->partition_key->attribute_value,
        *get_database_item_context.request->partition_key->attribute_value);
    EXPECT_EQ(*get_database_item_context.response->sort_key->attribute_name,
              *get_database_item_context.request->sort_key->attribute_name);
    EXPECT_EQ(*get_database_item_context.response->sort_key->attribute_value,
              *get_database_item_context.request->sort_key->attribute_value);
    EXPECT_EQ(
        *get_database_item_context.response->attributes->at(0).attribute_name,
        "attr1");
    EXPECT_EQ(get<string>(*get_database_item_context.response->attributes->at(0)
                               .attribute_value),
              "hello world");
    EXPECT_EQ(
        *get_database_item_context.response->attributes->at(1).attribute_name,
        "attr2");
    EXPECT_EQ(get<double>(*get_database_item_context.response->attributes->at(1)
                               .attribute_value),
              2);
    EXPECT_EQ(
        *get_database_item_context.response->attributes->at(2).attribute_name,
        "attr3");
    EXPECT_EQ(get<double>(*get_database_item_context.response->attributes->at(2)
                               .attribute_value),
              3);
    EXPECT_EQ(get_database_item_context.response->attributes->size(), 3);
  };

  QueryRequest query_request;
  QueryResult query_result;
  Map<String, AttributeValue> item;

  AttributeValue aws_partition_key;
  aws_partition_key.SetN("3");
  AttributeValue aws_sort_key;
  aws_sort_key.SetN("2");
  AttributeValue aws_attribute1;
  aws_attribute1.SetS("hello world");
  AttributeValue aws_attribute2;
  aws_attribute2.SetN("2");
  AttributeValue aws_attribute3;
  aws_attribute3.SetN("3");

  item.emplace(String("Col1"), aws_partition_key);
  item.emplace(String("Col2"), aws_sort_key);
  item.emplace(String("attr1"), aws_attribute1);
  item.emplace(String("attr2"), aws_attribute2);
  item.emplace(String("attr3"), aws_attribute3);

  query_result.AddItems(item);

  QueryOutcome outcome(query_result);

  dynamo_db.OnGetDatabaseItemCallback(get_database_item_context, NULL,
                                      query_request, outcome, nullptr);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, UpsertItemWithPartitionAndSortKeyWithoutAttributes) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  upsert_database_item_context.request =
      make_shared<UpsertDatabaseItemRequest>();
  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));

  dynamo_db_client->update_item_async_mock =
      [&](const UpdateItemRequest& upsert_request,
          const UpdateItemResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(string(upsert_request.GetTableName().c_str()),
                  *upsert_database_item_context.request->table_name);
        auto keys = upsert_request.GetKey();
        EXPECT_EQ(keys.size(), 2);
        AttributeValue partition_value;
        partition_value.SetN(3);
        EXPECT_EQ(keys[String("Col1")], partition_value);
        AttributeValue sort_value;
        sort_value.SetN(2);
        EXPECT_EQ(keys[String("Col2")], sort_value);
        EXPECT_EQ(upsert_request.GetUpdateExpression().length(), 0);
        EXPECT_EQ(upsert_request.GetConditionExpression().length(), 0);
      };

  dynamo_db.UpsertDatabaseItem(upsert_database_item_context);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, UpsertItemWithPartitionAndSortKeyWithNewAttributes) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  upsert_database_item_context.request =
      make_shared<UpsertDatabaseItemRequest>();
  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  upsert_database_item_context.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  NoSqlDatabaseKeyValuePair attribute_1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr1"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("1234")};

  NoSqlDatabaseKeyValuePair attribute_2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute_3{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr3"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(4.5)};

  upsert_database_item_context.request->new_attributes->push_back(attribute_1);
  upsert_database_item_context.request->new_attributes->push_back(attribute_2);
  upsert_database_item_context.request->new_attributes->push_back(attribute_3);

  dynamo_db_client->update_item_async_mock =
      [&](const UpdateItemRequest& upsert_request,
          const UpdateItemResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(string(upsert_request.GetTableName().c_str()),
                  *upsert_database_item_context.request->table_name);
        auto keys = upsert_request.GetKey();
        EXPECT_EQ(keys.size(), 2);
        AttributeValue partition_value;
        partition_value.SetN(3);
        EXPECT_EQ(keys[String("Col1")], partition_value);
        AttributeValue sort_value;
        sort_value.SetN(2);
        EXPECT_EQ(keys[String("Col2")], sort_value);
        EXPECT_NE(upsert_request.GetUpdateExpression().length(), 0);
        EXPECT_EQ(string(upsert_request.GetUpdateExpression().c_str()),
                  "SET Attr1= :new_attribute_0 , Attr2= :new_attribute_1 , "
                  "Attr3= :new_attribute_2");
        EXPECT_EQ(upsert_request.GetConditionExpression().length(), 0);
      };

  dynamo_db.UpsertDatabaseItem(upsert_database_item_context);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, UpsertItemWithPartitionAndSortKeyWithOldAttributes) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  upsert_database_item_context.request =
      make_shared<UpsertDatabaseItemRequest>();
  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  upsert_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  NoSqlDatabaseKeyValuePair attribute_1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr1"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("1234")};

  NoSqlDatabaseKeyValuePair attribute_2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute_3{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Attr3"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(4.5)};

  upsert_database_item_context.request->attributes->push_back(attribute_1);
  upsert_database_item_context.request->attributes->push_back(attribute_2);
  upsert_database_item_context.request->attributes->push_back(attribute_3);

  dynamo_db_client->update_item_async_mock =
      [&](const UpdateItemRequest& upsert_request,
          const UpdateItemResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(string(upsert_request.GetTableName().c_str()),
                  *upsert_database_item_context.request->table_name);
        auto keys = upsert_request.GetKey();
        EXPECT_EQ(keys.size(), 2);
        AttributeValue partition_value;
        partition_value.SetN(3);
        EXPECT_EQ(keys[String("Col1")], partition_value);
        AttributeValue sort_value;
        sort_value.SetN(2);
        EXPECT_EQ(keys[String("Col2")], sort_value);
        EXPECT_EQ(upsert_request.GetUpdateExpression().length(), 0);
        EXPECT_NE(upsert_request.GetConditionExpression().length(), 0);
        EXPECT_EQ(string(upsert_request.GetConditionExpression().c_str()),
                  "Attr1= :attribute_0 and Attr2= :attribute_1 and "
                  "Attr3= :attribute_2");
      };

  dynamo_db.UpsertDatabaseItem(upsert_database_item_context);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, UpsertDatabaseItemCallbackFailure) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  upsert_database_item_context.request =
      make_shared<UpsertDatabaseItemRequest>();
  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));

  upsert_database_item_context.callback =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              upsert_database_item_context) {
        EXPECT_THAT(upsert_database_item_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR)));
      };

  UpdateItemRequest update_item_request;
  AWSError<DynamoDBErrors> dynamo_db_error(DynamoDBErrors::BACKUP_IN_USE,
                                           false);

  UpdateItemOutcome outcome(dynamo_db_error);

  dynamo_db.OnUpsertDatabaseItemCallback(upsert_database_item_context, NULL,
                                         update_item_request, outcome, nullptr);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, UpsertDatabaseItemCallback) {
  SDKOptions options;
  InitAPI(options);

  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>();

  MockAwsDynamoDB dynamo_db(dynamo_db_client, mock_async_executor);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context;

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};
  upsert_database_item_context.request =
      make_shared<UpsertDatabaseItemRequest>();
  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));

  upsert_database_item_context.callback =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              upsert_database_item_context) {
        EXPECT_SUCCESS(upsert_database_item_context.result);
      };

  UpdateItemRequest update_item_request;
  UpdateItemResult update_item_response;

  UpdateItemOutcome outcome(update_item_response);

  dynamo_db.OnUpsertDatabaseItemCallback(upsert_database_item_context, NULL,
                                         update_item_request, outcome, nullptr);
  ShutdownAPI(options);
}

TEST(AwsDynamoDBTests, DISABLED_LeaseManagerWithleasableLockOnDynamoDB) {
  SDKOptions options;
  InitAPI(options);

  auto client_config = make_shared<Aws::Client::ClientConfiguration>();
  String aws_region("us-west-1");
  client_config->region = aws_region;
  auto dynamo_db_client = make_shared<MockAwsDynamoDBClient>(*client_config);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  auto dynamo_db =
      std::make_shared<MockAwsDynamoDB>(dynamo_db_client, mock_async_executor);

  static std::random_device random_device_local;
  static mt19937 random_generator(random_device_local());
  uniform_int_distribution<uint64_t> distribution;

  LeaseInfo lease_info;
  lease_info.lease_acquirer_id = std::to_string(distribution(random_generator));
  lease_info.service_endpoint_address =
      std::to_string(distribution(random_generator));

  LeaseManager lease_manager;
  auto leasable_lock = std::make_shared<LeasableLockOnNoSQLDatabase>(
      dynamo_db, lease_info, "pbs_lock_table_test");

  auto callback = [](LeaseTransitionType lease_transition,
                     std::optional<LeaseInfo> lease_info) {
    if (lease_transition == LeaseTransitionType::kAcquired) {
      std::cout << "Lease acquired: " << lease_info->service_endpoint_address
                << "\n";
    } else if (lease_transition == LeaseTransitionType::kRenewed) {
      std::cout << "Lease renewed: " << lease_info->service_endpoint_address
                << "\n";
    } else if (lease_transition == LeaseTransitionType::kLost) {
      std::cout << "Lease lost \n";
      std::abort();
    } else if (lease_transition == LeaseTransitionType::kNotAcquired) {
      std::cout << "Lease not acquired \n";
    }
  };

  EXPECT_SUCCESS(lease_manager.Init());
  EXPECT_SUCCESS(lease_manager.ManageLeaseOnLock(leasable_lock, callback));
  EXPECT_SUCCESS(lease_manager.Run());

  std::cout << "Lease manager is running...\n";

  std::this_thread::sleep_for(std::chrono::seconds(10000));
}
}  // namespace google::scp::core::test
