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

#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

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
  nosql_database_provider.in_memory_db->tables.Insert(
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

  sort_key->record = record;
}

TEST(MockNoSQLDatabaseProviderTests, GetItemWithPartitionAndSortKey) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(make_shared<GetDatabaseItemRequest>(),
                                [&](auto& context) {
                                  EXPECT_SUCCESS(context.result);
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
      *get_database_item_context.response->attributes->at(1).attribute_name,
      "attr2");
  EXPECT_EQ(
      *get_database_item_context.response->attributes->at(1).attribute_value,
      NoSQLDatabaseValidAttributeValueTypes(true));
  EXPECT_EQ(
      *get_database_item_context.response->attributes->at(0).attribute_name,
      "attr1");
  EXPECT_EQ(
      *get_database_item_context.response->attributes->at(0).attribute_value,
      NoSQLDatabaseValidAttributeValueTypes(4));
}

TEST(MockNoSQLDatabaseProviderTests, GetItemWithPartitionKey) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          make_shared<GetDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_THAT(
                context.result,
                ResultIs(FailureExecutionResult(
                    errors::
                        SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME)));
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
            EXPECT_THAT(
                context.result,
                ResultIs(FailureExecutionResult(
                    errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)));
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
      get_database_item_context(make_shared<GetDatabaseItemRequest>(),
                                [&](auto& context) {
                                  EXPECT_SUCCESS(context.result);
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
      upsert_database_item_context(make_shared<UpsertDatabaseItemRequest>(),
                                   [&](auto& context) {
                                     EXPECT_SUCCESS(context.result);
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
  EXPECT_SUCCESS(nosql_database_provider.in_memory_db->tables.Find(
      *upsert_database_item_context.request->table_name, table));

  auto db_partition = make_shared<MockNoSQLDatabaseProvider::Partition>();
  EXPECT_SUCCESS(table->partition_key_value.Find(
      *upsert_database_item_context.request->partition_key->attribute_value,
      db_partition));

  auto db_sort_key = make_shared<MockNoSQLDatabaseProvider::SortKey>();
  EXPECT_SUCCESS(db_partition->sort_key_value.Find(
      *upsert_database_item_context.request->sort_key->attribute_value,
      db_sort_key));

  NoSQLDatabaseValidAttributeValueTypes attribute_value;
  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(
      *new_attribute1.attribute_name, attribute_value));
  EXPECT_EQ(attribute_value, *new_attribute1.attribute_value);

  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(
      *new_attribute2.attribute_name, attribute_value));
  EXPECT_EQ(attribute_value, *new_attribute2.attribute_value);
}

TEST(MockNoSQLDatabaseProviderTests, UpsertExistingItem) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  InitializeInMemoryDatabase(nosql_database_provider);

  atomic<bool> condition = false;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(make_shared<UpsertDatabaseItemRequest>(),
                                   [&](auto& context) {
                                     EXPECT_SUCCESS(context.result);
                                     condition = true;
                                   });

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("Col2"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attr1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(6)};

  NoSqlDatabaseKeyValuePair attr2{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr2"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(false)};

  NoSqlDatabaseKeyValuePair attr1_modified{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(10000)};

  NoSqlDatabaseKeyValuePair attr2_modified{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr2"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr_string")};

  NoSqlDatabaseKeyValuePair attr3{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr3"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(10)};

  NoSqlDatabaseKeyValuePair attr4{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr4"),
      .attribute_value =
          make_shared<NoSQLDatabaseValidAttributeValueTypes>(true)};

  // Insert attributes attr1, attr2
  upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(partition_key);
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(sort_key);
  upsert_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->new_attributes->push_back(attr1);
  upsert_database_item_context.request->new_attributes->push_back(attr2);

  nosql_database_provider.UpsertDatabaseItem(upsert_database_item_context);
  WaitUntil([&]() { return condition.load(); });

  // Verify
  auto table = make_shared<MockNoSQLDatabaseProvider::Table>();
  EXPECT_SUCCESS(nosql_database_provider.in_memory_db->tables.Find(
      *upsert_database_item_context.request->table_name, table));

  auto db_partition = make_shared<MockNoSQLDatabaseProvider::Partition>();
  EXPECT_SUCCESS(table->partition_key_value.Find(
      *upsert_database_item_context.request->partition_key->attribute_value,
      db_partition));

  auto db_sort_key = make_shared<MockNoSQLDatabaseProvider::SortKey>();
  EXPECT_SUCCESS(db_partition->sort_key_value.Find(
      *upsert_database_item_context.request->sort_key->attribute_value,
      db_sort_key));

  NoSQLDatabaseValidAttributeValueTypes attribute_value;
  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(*attr1.attribute_name,
                                                      attribute_value));
  EXPECT_EQ(attribute_value, *attr1.attribute_value);

  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(*attr2.attribute_name,
                                                      attribute_value));
  EXPECT_EQ(attribute_value, *attr2.attribute_value);

  // Upsert conditionally on attr1, attr2, to insert two new attributes and
  // modify existing attr1, attr2 as well.
  atomic<bool> conditional_upsert_done = false;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      conditional_upsert_database_item_context(
          make_shared<UpsertDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_SUCCESS(context.result);
            conditional_upsert_done = true;
          });
  conditional_upsert_database_item_context.request->table_name =
      make_shared<string>("TestTable");
  conditional_upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(partition_key);
  conditional_upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(sort_key);
  conditional_upsert_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  conditional_upsert_database_item_context.request->attributes->push_back(
      attr1);
  conditional_upsert_database_item_context.request->attributes->push_back(
      attr2);
  conditional_upsert_database_item_context.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  conditional_upsert_database_item_context.request->new_attributes->push_back(
      attr1_modified);
  conditional_upsert_database_item_context.request->new_attributes->push_back(
      attr2_modified);
  conditional_upsert_database_item_context.request->new_attributes->push_back(
      attr3);
  conditional_upsert_database_item_context.request->new_attributes->push_back(
      attr4);

  nosql_database_provider.UpsertDatabaseItem(
      conditional_upsert_database_item_context);
  WaitUntil([&]() { return conditional_upsert_done.load(); });

  // Verify
  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(*attr1.attribute_name,
                                                      attribute_value));
  EXPECT_EQ(attribute_value, NoSQLDatabaseValidAttributeValueTypes(10000));

  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(*attr2.attribute_name,
                                                      attribute_value));
  EXPECT_EQ(attribute_value,
            NoSQLDatabaseValidAttributeValueTypes("attr_string"));
  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(*attr3.attribute_name,
                                                      attribute_value));
  EXPECT_EQ(attribute_value, NoSQLDatabaseValidAttributeValueTypes(10));

  EXPECT_SUCCESS(db_sort_key->record->attributes.Find(*attr4.attribute_name,
                                                      attribute_value));
  EXPECT_EQ(attribute_value, NoSQLDatabaseValidAttributeValueTypes(true));
}

TEST(MockNoSQLDatabaseProviderTests, TestInitializeTable) {
  MockNoSQLDatabaseProvider nosql_database_provider;
  nosql_database_provider.InitializeTable("testing_table", "part_key",
                                          "sort_key");

  NoSqlDatabaseKeyValuePair partition_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("part_key"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(3)};

  NoSqlDatabaseKeyValuePair sort_key{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("sort_key"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(2)};

  NoSqlDatabaseKeyValuePair attr1{
      .attribute_name = make_shared<NoSQLDatabaseAttributeName>("attr1"),
      .attribute_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>(6)};

  atomic<bool> condition = false;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(make_shared<UpsertDatabaseItemRequest>(),
                                   [&](auto& context) {
                                     EXPECT_SUCCESS(context.result);
                                     condition = true;
                                   });
  upsert_database_item_context.request->table_name =
      make_shared<string>("testing_table");
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>(partition_key);
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>(sort_key);
  upsert_database_item_context.request->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.request->new_attributes->push_back(attr1);

  nosql_database_provider.UpsertDatabaseItem(upsert_database_item_context);
  WaitUntil([&]() { return condition.load(); });
}

}  // namespace google::scp::core::test
