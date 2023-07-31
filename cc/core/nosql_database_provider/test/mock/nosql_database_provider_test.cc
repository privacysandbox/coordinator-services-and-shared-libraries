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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/test/utils/conditional_wait.h"

using google::scp::core::common::ConcurrentMap;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::make_pair;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::core::test {

void InitializeInMemoryDatabase(
    MockNoSQLDatabaseProvider& nosql_database_provider) {
  auto table = make_shared<MockNoSQLDatabaseProvider::Table>();
  nosql_database_provider.in_memory_map.tables.Insert(
      make_pair(string("TestTable"), table), table);

  // Insert partition
  auto partition = make_shared<MockNoSQLDatabaseProvider::Partition>();
  table->partition_key_name = make_shared<NoSQLDatabaseAttributeName>("Col1");
  table->sort_key_name = make_shared<NoSQLDatabaseAttributeName>("Col2");
  table->partition_key_value.Insert(make_pair(1, partition), partition);

  // Insert sort key
  auto sort_key = make_shared<MockNoSQLDatabaseProvider::SortKey>();
  partition->sort_key_value.Insert(make_pair(2, sort_key), sort_key);

  // Insert Record
  auto record = make_shared<MockNoSQLDatabaseProvider::Record>();

  NoSQLDatabaseAttributeName attribute_key = "attr1";
  NoSQLDatabaseValidAttributeValueTypes attribute_value = 4;
  record->attributes.Insert(make_pair(attribute_key, attribute_value),
                            attribute_value);

  attribute_key = "attr2";
  attribute_value = true;
  record->attributes.Insert(make_pair(attribute_key, attribute_value),
                            attribute_value);

  sort_key->sorted_records.push_back(record);
}

TEST(MockNoSQLDatabaseProviderTests, GetItemWithPartitionAndSortKey) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          make_shared<GetDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_EQ(context.result, SuccessExecutionResult());
            condition = true;
          });

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(1)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(4)};

  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  get_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  get_database_item_context.request->attributes->push_back(attribute);

  nosql_database_provider.GetDatabaseItem(get_database_item_context);
  WaitUntil([&]() { return condition.load(); });

  EXPECT_EQ(*get_database_item_context.response->partition_key->attribute_name,
            "Col1");
  EXPECT_EQ(*get_database_item_context.response->partition_key->attribute_value,
            NoSQLDatabaseValidAttributeValueTypes(1));
  EXPECT_EQ(*get_database_item_context.response->sort_key->attribute_name,
            "Col2");
  EXPECT_EQ(*get_database_item_context.response->sort_key->attribute_value,
            NoSQLDatabaseValidAttributeValueTypes(2));
  EXPECT_EQ(get_database_item_context.response->attributes->size(), 2);
  EXPECT_EQ(
      *get_database_item_context.response->attributes->at(0).attribute_name,
      "attr2");
  EXPECT_EQ(
      *get_database_item_context.response->attributes->at(0).attribute_value,
      NoSQLDatabaseValidAttributeValueTypes(true));
  EXPECT_EQ(
      *get_database_item_context.response->attributes->at(1).attribute_name,
      "attr1");
  EXPECT_EQ(
      *get_database_item_context.response->attributes->at(1).attribute_value,
      NoSQLDatabaseValidAttributeValueTypes(4));
}

TEST(MockNoSQLDatabaseProviderTests, GetItemWithPartitionKey) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          make_shared<GetDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_EQ(context.result,
                      FailureExecutionResult(
                          errors::SC_NO_SQL_DATABASE_INVALID_REQUEST));
            condition = true;
          });

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(1)};

  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));

  nosql_database_provider.GetDatabaseItem(get_database_item_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST(MockNoSQLDatabaseProviderTests, PartitionNotFound) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          make_shared<GetDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_EQ(
                context.result,
                FailureExecutionResult(
                    errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND));
            condition = true;
          });

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(4)};

  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  get_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  get_database_item_context.request->attributes->push_back(attribute);

  nosql_database_provider.GetDatabaseItem(get_database_item_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST(MockNoSQLDatabaseProviderTests, AttributeNotFound) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          make_shared<GetDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_EQ(context.result, SuccessExecutionResult());
            condition = true;
          });

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(1)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(56)};

  get_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  get_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  get_database_item_context.request->attributes->push_back(attribute);

  nosql_database_provider.GetDatabaseItem(get_database_item_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST(MockNoSQLDatabaseProviderTests, UpsertNonExistingItem) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(
          make_shared<UpsertDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_EQ(context.result, SuccessExecutionResult());
            condition = true;
          });

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair new_attribute1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr12"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(24)};

  NoSqlDatabaseKeyValuePair new_attribute2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr51"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(45)};

  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  upsert_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->new_attributes->push_back(
      new_attribute1);
  upsert_database_item_context.request->new_attributes->push_back(
      new_attribute2);

  nosql_database_provider.UpsertDatabaseItem(upsert_database_item_context);
  WaitUntil([&]() { return condition.load(); });

  // Find the table
  auto table = make_shared<MockNoSQLDatabaseProvider::Table>();
  EXPECT_EQ(nosql_database_provider.in_memory_map.tables.Find(
                *upsert_database_item_context.request->table_name, table),
            SuccessExecutionResult());

  auto db_partition = make_shared<MockNoSQLDatabaseProvider::Partition>();
  EXPECT_EQ(
      table->partition_key_value.Find(
          *upsert_database_item_context.request->partition_key->attribute_value,
          db_partition),
      SuccessExecutionResult());

  auto db_sort_key = make_shared<MockNoSQLDatabaseProvider::SortKey>();
  EXPECT_EQ(
      db_partition->sort_key_value.Find(
          *upsert_database_item_context.request->sort_key->attribute_value,
          db_sort_key),
      SuccessExecutionResult());

  NoSQLDatabaseValidAttributeValueTypes attribute_value;
  EXPECT_EQ(db_sort_key->sorted_records[0]->attributes.Find(
                *new_attribute1.attribute_name, attribute_value),
            SuccessExecutionResult());
  EXPECT_EQ(attribute_value, *new_attribute1.attribute_value);

  EXPECT_EQ(db_sort_key->sorted_records[0]->attributes.Find(
                *new_attribute2.attribute_name, attribute_value),
            SuccessExecutionResult());
  EXPECT_EQ(attribute_value, *new_attribute2.attribute_value);
}

TEST(MockNoSQLDatabaseProviderTests, UpsertExistingItem) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(
          make_shared<UpsertDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_EQ(context.result, SuccessExecutionResult());
            condition = true;
          });

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attribute1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(4)};

  NoSqlDatabaseKeyValuePair attribute2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr2"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(true)};

  NoSqlDatabaseKeyValuePair new_attribute1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(6)};

  NoSqlDatabaseKeyValuePair new_attribute2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr2"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(false)};

  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(partition_key));
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(move(sort_key));
  upsert_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->attributes->push_back(new_attribute1);
  upsert_database_item_context.request->attributes->push_back(new_attribute2);
  upsert_database_item_context.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->new_attributes->push_back(
      new_attribute1);
  upsert_database_item_context.request->new_attributes->push_back(
      new_attribute2);

  nosql_database_provider.UpsertDatabaseItem(upsert_database_item_context);
  WaitUntil([&]() { return condition.load(); });

  // Find the table
  auto table = make_shared<MockNoSQLDatabaseProvider::Table>();
  EXPECT_EQ(nosql_database_provider.in_memory_map.tables.Find(
                *upsert_database_item_context.request->table_name, table),
            SuccessExecutionResult());

  auto db_partition = make_shared<MockNoSQLDatabaseProvider::Partition>();
  EXPECT_EQ(
      table->partition_key_value.Find(
          *upsert_database_item_context.request->partition_key->attribute_value,
          db_partition),
      SuccessExecutionResult());

  auto db_sort_key = make_shared<MockNoSQLDatabaseProvider::SortKey>();
  EXPECT_EQ(
      db_partition->sort_key_value.Find(
          *upsert_database_item_context.request->sort_key->attribute_value,
          db_sort_key),
      SuccessExecutionResult());

  NoSQLDatabaseValidAttributeValueTypes attribute_value;
  EXPECT_EQ(db_sort_key->sorted_records[0]->attributes.Find(
                *attribute1.attribute_name, attribute_value),
            SuccessExecutionResult());
  EXPECT_EQ(attribute_value, *new_attribute1.attribute_value);

  EXPECT_EQ(db_sort_key->sorted_records[0]->attributes.Find(
                *attribute2.attribute_name, attribute_value),
            SuccessExecutionResult());
  EXPECT_EQ(attribute_value, *new_attribute2.attribute_value);
}
}  // namespace google::scp::core::test
