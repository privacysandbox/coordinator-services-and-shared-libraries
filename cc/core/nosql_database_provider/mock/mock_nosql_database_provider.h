/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/error_codes.h"

namespace google::scp::core::nosql_database_provider::mock {
class MockNoSQLDatabaseProvider : public NoSQLDatabaseProviderInterface {
 public:
  struct Record {
    core::common::ConcurrentMap<NoSQLDatabaseAttributeName,
                                NoSQLDatabaseValidAttributeValueTypes>
        attributes;
  };

  struct SortKey {
    std::mutex sort_key_lock;
    std::vector<std::shared_ptr<Record>> sorted_records;
  };

  struct Partition {
    core::common::ConcurrentMap<NoSQLDatabaseValidAttributeValueTypes,
                                std::shared_ptr<SortKey>>
        sort_key_value;
  };

  struct Table {
    std::shared_ptr<NoSQLDatabaseAttributeName> partition_key_name;
    std::shared_ptr<NoSQLDatabaseAttributeName> sort_key_name;

    core::common::ConcurrentMap<NoSQLDatabaseValidAttributeValueTypes,
                                std::shared_ptr<Partition>>
        partition_key_value;
  };

  struct InMemoryDatabase {
    core::common::ConcurrentMap<std::string, std::shared_ptr<Table>> tables;
  };

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult GetDatabaseItem(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
          get_database_item_context) noexcept override {
    if (!get_database_item_context.request ||
        !get_database_item_context.request->table_name ||
        !get_database_item_context.request->sort_key ||
        !get_database_item_context.request->partition_key ||
        !get_database_item_context.request->attributes ||
        get_database_item_context.request->attributes->size() == 0) {
      get_database_item_context.result =
          FailureExecutionResult(errors::SC_NO_SQL_DATABASE_INVALID_REQUEST);
      get_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find the table
    auto table = std::make_shared<Table>();
    auto execution_result = in_memory_map.tables.Find(
        *get_database_item_context.request->table_name, table);
    if (!execution_result.Successful()) {
      get_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_TABLE_NOT_FOUND);
      get_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find the partition key name
    if (*table->partition_key_name !=
        *get_database_item_context.request->partition_key->attribute_name) {
      get_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME);
      get_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find the partition value
    auto partition = std::make_shared<Partition>();
    if (!table->partition_key_value
             .Find(*get_database_item_context.request->partition_key
                        ->attribute_value,
                   partition)
             .Successful()) {
      get_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
      get_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find the sort key name
    if (*table->sort_key_name !=
        *get_database_item_context.request->sort_key->attribute_name) {
      get_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME);
      get_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    auto sort_key = std::make_shared<SortKey>();
    if (!partition->sort_key_value
             .Find(
                 *get_database_item_context.request->sort_key->attribute_value,
                 sort_key)
             .Successful()) {
      get_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
      get_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    sort_key->sort_key_lock.lock();
    auto record = std::make_shared<Record>();
    execution_result = SearchSortkey(
        sort_key, get_database_item_context.request->attributes, record);
    if (!execution_result.Successful()) {
      get_database_item_context.result = execution_result;
      get_database_item_context.Finish();
      return SuccessExecutionResult();
    }
    sort_key->sort_key_lock.unlock();

    get_database_item_context.response =
        std::make_shared<GetDatabaseItemResponse>();
    get_database_item_context.response->partition_key =
        get_database_item_context.request->partition_key;
    get_database_item_context.response->sort_key =
        get_database_item_context.request->sort_key;
    get_database_item_context.response->attributes =
        std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();

    std::vector<NoSQLDatabaseAttributeName> attribute_keys;
    record->attributes.Keys(attribute_keys);

    for (auto attribute_key : attribute_keys) {
      NoSQLDatabaseValidAttributeValueTypes attribute_value;
      record->attributes.Find(attribute_key, attribute_value);

      NoSqlDatabaseKeyValuePair attribute;
      attribute.attribute_name =
          std::make_shared<NoSQLDatabaseAttributeName>(attribute_key);
      attribute.attribute_value =
          std::make_shared<NoSQLDatabaseValidAttributeValueTypes>(
              attribute_value);
      get_database_item_context.response->attributes->push_back(attribute);
    }

    get_database_item_context.result = SuccessExecutionResult();
    get_database_item_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult UpsertDatabaseItem(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept override {
    if (upsert_database_item_mock) {
      return upsert_database_item_mock(upsert_database_item_context);
    }

    if (!upsert_database_item_context.request ||
        !upsert_database_item_context.request->table_name ||
        !upsert_database_item_context.request->partition_key ||
        !upsert_database_item_context.request->attributes ||
        !upsert_database_item_context.request->new_attributes ||
        upsert_database_item_context.request->new_attributes->size() == 0) {
      upsert_database_item_context.result =
          FailureExecutionResult(errors::SC_NO_SQL_DATABASE_INVALID_REQUEST);
      upsert_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find the table
    auto table = std::make_shared<Table>();
    auto execution_result = in_memory_map.tables.Find(
        *upsert_database_item_context.request->table_name, table);
    if (!execution_result.Successful()) {
      upsert_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_TABLE_NOT_FOUND);
      upsert_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find the partition key name
    if (*table->partition_key_name !=
        *upsert_database_item_context.request->partition_key->attribute_name) {
      upsert_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME);
      upsert_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find/Insert the partition key
    auto partition = std::make_shared<Partition>();
    execution_result = table->partition_key_value.Insert(
        std::make_pair(*upsert_database_item_context.request->partition_key
                            ->attribute_value,
                       partition),
        partition);
    if (!execution_result.Successful()) {
      // It is fine we can continue;
    }

    // Find the sort key name
    if (*table->sort_key_name !=
        *upsert_database_item_context.request->sort_key->attribute_name) {
      upsert_database_item_context.result = FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME);
      upsert_database_item_context.Finish();
      return SuccessExecutionResult();
    }

    // Find/Insert the sort key
    auto sort_key = std::make_shared<SortKey>();
    auto sort_key_value =
        upsert_database_item_context.request->sort_key->attribute_value;
    execution_result = partition->sort_key_value.Insert(
        std::make_pair(*sort_key_value, sort_key), sort_key);
    if (!execution_result.Successful()) {
      // Already exist and it is fine.
    }

    // Upsert key.
    sort_key->sort_key_lock.lock();
    auto record = std::make_shared<Record>();
    execution_result = SearchSortkey(
        sort_key, upsert_database_item_context.request->attributes, record);

    if (execution_result.Successful()) {
      for (auto new_attribute :
           *upsert_database_item_context.request->new_attributes) {
        record->attributes.Erase(*new_attribute.attribute_name);
      }
    } else {
      sort_key->sorted_records.push_back(record);
    }

    for (auto attribute :
         *upsert_database_item_context.request->new_attributes) {
      NoSQLDatabaseValidAttributeValueTypes record_attribute_value = 0;
      auto execution_result = record->attributes.Insert(
          std::make_pair(*attribute.attribute_name, *attribute.attribute_value),
          record_attribute_value);
      if (!execution_result.Successful()) {
        upsert_database_item_context.result = execution_result;
        upsert_database_item_context.Finish();
        return SuccessExecutionResult();
      }
    }
    sort_key->sort_key_lock.unlock();

    upsert_database_item_context.response =
        std::make_shared<UpsertDatabaseItemResponse>();
    upsert_database_item_context.response->partition_key =
        upsert_database_item_context.request->partition_key;
    upsert_database_item_context.response->sort_key =
        upsert_database_item_context.request->sort_key;
    upsert_database_item_context.response->attributes =
        std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();

    std::vector<NoSQLDatabaseAttributeName> attribute_keys;
    record->attributes.Keys(attribute_keys);

    for (auto attribute_key : attribute_keys) {
      NoSQLDatabaseValidAttributeValueTypes attribute_value;
      record->attributes.Find(attribute_key, attribute_value);

      NoSqlDatabaseKeyValuePair attribute;
      attribute.attribute_name =
          std::make_shared<NoSQLDatabaseAttributeName>(attribute_key);
      attribute.attribute_value =
          std::make_shared<NoSQLDatabaseValidAttributeValueTypes>(
              attribute_value);
      upsert_database_item_context.response->attributes->push_back(attribute);
    }

    upsert_database_item_context.result = SuccessExecutionResult();
    upsert_database_item_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult SearchSortkey(
      std::shared_ptr<SortKey>& sort_key,
      std::shared_ptr<std::vector<NoSqlDatabaseKeyValuePair>>& attributes,
      std::shared_ptr<Record>& out_record) {
    for (std::shared_ptr<Record> record : sort_key->sorted_records) {
      bool no_match = false;
      for (auto attribute : *attributes) {
        NoSQLDatabaseValidAttributeValueTypes attribute_value;
        if (record->attributes.Find(*attribute.attribute_name,
                                    attribute_value) !=
            SuccessExecutionResult()) {
          no_match = true;
          break;
        }
      }

      if (!no_match) {
        out_record = record;
        return SuccessExecutionResult();
      }
    }
    return FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
  }

  InMemoryDatabase in_memory_map;
  std::function<ExecutionResult(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&)>
      upsert_database_item_mock;
};
}  // namespace google::scp::core::nosql_database_provider::mock
