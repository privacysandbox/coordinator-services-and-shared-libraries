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
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "async_context.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {
using NoSQLDatabaseValidAttributeValueTypes =
    std::variant<int, float, double, std::string>;
using NoSQLDatabaseAttributeName = std::string;

/// Types of NoSLQ Keys
enum class NoSqlDatabaseKeyType { PartitionKey = 0, SortKey = 1, None = 2 };

/// Represents KeyValue pair for a NoSQL Attribute.
struct NoSqlDatabaseKeyValuePair {
  /// A shared pointer to the attribute name.
  std::shared_ptr<NoSQLDatabaseAttributeName> attribute_name;
  /// A shared pointer to the attribute value.
  std::shared_ptr<NoSQLDatabaseValidAttributeValueTypes> attribute_value;
};

/// Holds common key metadata for single item operation requests.
struct SingleDatabaseItemRequest {
  virtual ~SingleDatabaseItemRequest() = default;

  /// The NoSQL table name.
  std::shared_ptr<std::string> table_name;
  /// The record partition key.
  std::shared_ptr<NoSqlDatabaseKeyValuePair> partition_key;
  /// The record sort key (index).
  std::shared_ptr<NoSqlDatabaseKeyValuePair> sort_key;
  /// Attributes associated with the record.
  std::shared_ptr<std::vector<NoSqlDatabaseKeyValuePair>> attributes;
};

/// Holds common key metadata for single item operation responses.
struct SingleDatabaseItemResponse {
  virtual ~SingleDatabaseItemResponse() = default;

  // The NoSQL table name.
  std::shared_ptr<std::string> table_name;
  /// The record partition key.
  std::shared_ptr<NoSqlDatabaseKeyValuePair> partition_key;
  /// The record sort key (index).
  std::shared_ptr<NoSqlDatabaseKeyValuePair> sort_key;
  /// Attributes associated with the record.
  std::shared_ptr<std::vector<NoSqlDatabaseKeyValuePair>> attributes;
};

/// Get database item request object.
struct GetDatabaseItemRequest : SingleDatabaseItemRequest {};

/// Get database item response object.
struct GetDatabaseItemResponse : SingleDatabaseItemResponse {};

/// Upsert database item request object.
struct UpsertDatabaseItemRequest : SingleDatabaseItemRequest {
  /// Attributes associated with the upsert record.
  std::shared_ptr<std::vector<NoSqlDatabaseKeyValuePair>> new_attributes;
};

/// Upsert database item response object.
struct UpsertDatabaseItemResponse : SingleDatabaseItemRequest {};

/**
 * @brief NoSQLDatabase provides database access APIs for single records.
 */
class NoSQLDatabaseProviderInterface : public ServiceInterface {
 public:
  virtual ~NoSQLDatabaseProviderInterface() = default;

  /**
   * @brief Gets a database record using provided metadta.
   *
   * @param get_database_item_context The context object for the database
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult GetDatabaseItem(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
          get_database_item_context) noexcept = 0;

  /**
   * @brief Upserts a database record using provided metadta.
   *
   * @param get_database_item_context The context object for the database
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult UpsertDatabaseItem(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept = 0;
};
}  // namespace google::scp::core
