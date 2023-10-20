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
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mutations.h"

namespace google::scp::cpio::client_providers {

class SpannerFactory;

/*! @copydoc NoSQLDatabaseProviderInterface
 */
class GcpSpannerClientProvider : public NoSQLDatabaseClientProviderInterface {
 public:
  /**
   * @brief Construct a new Gcp Spanner Client Provider object
   *
   * @param client_options Options for the Client.
   * @param instance_client Client used for getting the Project ID to work with.
   * @param cpu_async_executor Executor to run the async operations on.
   * @param io_async_executor Executor to run the IO async operations on.
   * @param spanner_factory Factory which provides the Spanner Client.
   */
  GcpSpannerClientProvider(
      std::shared_ptr<NoSQLDatabaseClientOptions> client_options,
      std::shared_ptr<InstanceClientProviderInterface> instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      std::shared_ptr<SpannerFactory> spanner_factory =
          std::make_shared<SpannerFactory>())
      : client_options_(client_options),
        instance_client_(instance_client),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        spanner_factory_(spanner_factory) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult CreateTable(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::CreateTableRequest,
          cmrt::sdk::nosql_database_service::v1::CreateTableResponse>&
          create_table_context) noexcept override;

  core::ExecutionResult DeleteTable(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::DeleteTableRequest,
          cmrt::sdk::nosql_database_service::v1::DeleteTableResponse>&
          delete_table_context) noexcept override;

  core::ExecutionResult GetDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept override;

  core::ExecutionResult UpsertDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept override;

 private:
  /**
   * @brief Is called by async executor in order to create the table.
   *
   * @param create_table_context The context object of the create table
   * operation.
   */
  void CreateTableAsync(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::CreateTableRequest,
          cmrt::sdk::nosql_database_service::v1::CreateTableResponse>&
          create_table_context) noexcept;
  /**
   * @brief Is called by async executor in order to delete the table.
   *
   * @param delete_table_context The context object of the delete table
   * operation.
   */
  void DeleteTableAsync(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::DeleteTableRequest,
          cmrt::sdk::nosql_database_service::v1::DeleteTableResponse>&
          delete_table_context) noexcept;

  /**
   * @brief Is called by async executor in order to acquire the DB item.
   *
   * @param get_database_item_context The context object of the get database
   * item operation.
   * @param query The query to execute to get the DB item.
   * @param params The parameters to be substituted into query.
   */
  void GetDatabaseItemAsync(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>
          get_database_item_context,
      std::string query,
      google::cloud::spanner::SqlStatement::ParamType params) noexcept;

  struct UpsertSelectOptions {
    // Method to create an UpsertSelectOptions given an upsert request.
    static core::ExecutionResultOr<UpsertSelectOptions>
    BuildUpsertSelectOptions(
        const cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest&
            request);

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
   * @param upsert_database_item_context Context used for logging errors.
   * @param row_stream The row stream to read the existing row out of.
   * @param enforce_row_existence If pre-attributes are provided, then we must
   * ensure our select_statement produces a row before proceeding.
   * @param new_attributes A JSON holding the new_attributes we extracted
   * from UpsertDatabaseItemRequest.
   * @param spanner_json
   * @return ExecutionResult
   */
  core::ExecutionResult GetMergedJson(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context,
      google::cloud::spanner::RowStream& row_stream, bool enforce_row_existence,
      nlohmann::json new_attributes,
      std::optional<google::cloud::spanner::Json>& spanner_json);

  // Used to pass to client.Commit when Upserting an item.
  google::cloud::spanner::Mutations UpsertFunctor(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context,
      google::cloud::spanner::Client& client,
      const UpsertSelectOptions& upsert_select_options,
      bool enforce_row_existence, const nlohmann::json& new_attributes,
      core::ExecutionResult& prepare_result, const std::string& table_name,
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
  void UpsertDatabaseItemAsync(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>
          upsert_database_item_context,
      UpsertSelectOptions upsert_select_options, bool enforce_row_existence,
      nlohmann::json new_attributes) noexcept;

  /// Options for the client.
  std::shared_ptr<NoSQLDatabaseClientOptions> client_options_;

  /// Instance client.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_;

  /// An instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  /// An instance of the GCP Spanner client. To enable thread safety of the
  /// encapsulating class, this instance is marked as const so it can't actually
  /// be used to query Spanner. Instead, Get/Upsert create a copy of this Client
  /// (which is a cheap operation) to query Spanner.
  std::shared_ptr<const google::cloud::spanner::Client> spanner_client_shared_;

  /// An instance of the GCP Spanner Database client. To enable thread safety of
  /// the encapsulating class, this instance is marked as const so it can't
  /// actually be used to query Spanner. Instead, Create/Delete create a copy of
  /// this Client (which is a cheap operation) to update Spanner Databases.
  std::shared_ptr<const google::cloud::spanner_admin::DatabaseAdminClient>
      spanner_database_client_shared_;

  /// spanner_database_client_shared_ expects the database name in the form:
  /// projects/<PROJECT>/instances/<INSTANCE>/databases/<DATABASE>.
  std::string fully_qualified_db_name_;

  std::shared_ptr<SpannerFactory> spanner_factory_;
};

/// Creates GCP cloud::spanner::Client and
/// cloud::spanner_admin::DatabaseAdminClient.
class SpannerFactory {
 public:
  virtual core::ExecutionResultOr<std::pair<
      std::shared_ptr<google::cloud::spanner::Client>,
      std::shared_ptr<google::cloud::spanner_admin::DatabaseAdminClient>>>
  CreateClients(const std::string& project, const std::string& instance,
                const std::string& database) noexcept;

  virtual ~SpannerFactory() = default;
};

}  // namespace google::scp::cpio::client_providers
