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
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mutations.h"

namespace google::scp::core::nosql_database_provider {
/*! @copydoc NoSQLDatabaseProviderInterface
 */
class GcpSpanner : public NoSQLDatabaseProviderInterface {
 public:
  /**
   * @brief Construct a new Gcp Spanner object
   *
   * @param async_executor
   * @param io_async_executor
   * @param config_provider
   * @param table_name_to_keys Optional argument mapping names of tables to
   * the corresponding partition and (optional) sort keys for that table.
   * This is used to validate calls to Get* and Upsert*.
   */
  GcpSpanner(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      std::unique_ptr<std::unordered_map<
          std::string, std::pair<std::string, std::optional<std::string>>>>
          table_name_to_keys,
      AsyncPriority async_execution_priority,
      AsyncPriority io_async_execution_priority)
      : async_executor_(async_executor),
        io_async_executor_(io_async_executor),
        config_provider_(config_provider),
        table_name_to_keys_(std::move(table_name_to_keys)),
        async_execution_priority_(async_execution_priority),
        io_async_execution_priority_(io_async_execution_priority) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult GetDatabaseItem(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
          get_database_item_context) noexcept override;

  ExecutionResult UpsertDatabaseItem(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept override;

 protected:
  virtual void CreateSpanner(const std::string& project,
                             const std::string& instance,
                             const std::string& database) noexcept;

  explicit GcpSpanner(
      std::shared_ptr<google::cloud::spanner::Client> spanner_client,
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::unique_ptr<std::unordered_map<
          std::string, std::pair<std::string, std::optional<std::string>>>>
          table_name_to_keys,
      AsyncPriority async_execution_priority,
      AsyncPriority io_async_execution_priority)
      : async_executor_(async_executor),
        io_async_executor_(async_executor),
        spanner_client_shared_(std::move(spanner_client)),
        table_name_to_keys_(std::move(table_name_to_keys)),
        async_execution_priority_(async_execution_priority),
        io_async_execution_priority_(io_async_execution_priority) {}

  /**
   * @brief Is called by async executor in order to acquire the DB item.
   *
   * @param get_database_item_context The context object of the get database
   * item operation.
   * @param query The query to execute to get the DB item.
   * @param params The parameters to be substituted into query.
   */
  virtual void GetDatabaseItemAsync(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
          get_database_item_context,
      std::string query,
      google::cloud::spanner::SqlStatement::ParamType params) noexcept;

  struct UpsertSelectOptions {
    static ExecutionResultOr<UpsertSelectOptions> BuildUpsertSelectOptions(
        const UpsertDatabaseItemRequest& request);
    // The SELECT statement used to query the database for the current value
    // matching partition_key_val and sort_key_val.
    google::cloud::spanner::SqlStatement select_statement;
    // The names of all the columns we set in our InsertOrUpdateMutation.
    std::vector<std::string> column_names;
    // The spanner Value (string) for the partition_key and sort_key
    // respectively.
    google::cloud::spanner::Value partition_key_val, sort_key_val;

   private:
    UpsertSelectOptions() = default;
  };

  /**
   * @brief Extracts the acquired JSON value from row_stream, then merges it
   * with the json in read_modify_write_options and writes it into spanner_json.
   *
   * @param row_stream The row stream to read the existing row out of.
   * @param enforce_row_existence If pre-attributes are provided, then we must
   * ensure our select_statement produces a row before proceeding.
   * @param new_attributes A JSON holding the new_attributes we extracted
   * from UpsertDatabaseItemRequest.
   * @param spanner_json
   * @return ExecutionResult
   */
  static ExecutionResult GetMergedJson(
      google::cloud::spanner::RowStream& row_stream, bool enforce_row_existence,
      nlohmann::json new_attributes,
      std::optional<google::cloud::spanner::Json>& spanner_json);

  // Used to pass to client.Commit when Upserting an item.
  static google::cloud::spanner::Mutations UpsertFunctor(
      google::cloud::spanner::Client& client,
      const UpsertSelectOptions& upsert_select_options,
      bool enforce_row_existence, const nlohmann::json& new_attributes,
      ExecutionResult& prepare_result, const std::string& table_name,
      google::cloud::spanner::Transaction txn);

  /**
   * @brief Is called by async executor in order to upsert the DB item.
   *
   * @param upsert_database_item_context The context object of the upsert
   * database item operation.
   * @param read_modify_write_options The options for upserting the item.
   * @param enforce_row_existence If pre-attributes are provided, then we must
   * ensure our select_statement produces a row before proceeding.
   * @param new_attributes A JSON holding the new_attributes we extracted
   * from UpsertDatabaseItemRequest.
   */
  virtual void UpsertDatabaseItemAsync(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
          upsert_database_item_context,
      UpsertSelectOptions upsert_select_options, bool enforce_row_existence,
      nlohmann::json new_attributes) noexcept;

  /// An instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the IO async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;

  /// An instance of the GCP Spanner client. To enable thread safety of the
  /// encapsulating class, this instance is marked as const so it can't actually
  /// be used to query Spanner. Instead, Get/Upsert create a copy of this Client
  /// (which is a cheap operation) to query Spanner.
  std::shared_ptr<const google::cloud::spanner::Client> spanner_client_shared_;

  /// An instance of the config provider.
  const std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// Map of table name to (partition_key_name[, sort_key_name]) used for
  /// validating calls to Get* and Upsert*.
  const std::unique_ptr<const std::unordered_map<
      std::string, std::pair<std::string, std::optional<std::string>>>>
      table_name_to_keys_;

  /// Priorities with which the tasks will be scheduled onto the async
  /// executors.
  const AsyncPriority async_execution_priority_;
  const AsyncPriority io_async_execution_priority_;
};
}  // namespace google::scp::core::nosql_database_provider
