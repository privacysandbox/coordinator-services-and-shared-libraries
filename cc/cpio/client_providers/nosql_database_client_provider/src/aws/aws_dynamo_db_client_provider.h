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

#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>

#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"

namespace google::scp::cpio::client_providers {

class DynamoDBFactory;

/*! @copydoc NoSQLDatabaseClientProviderInterface
 */
class AwsDynamoDBClientProvider : public NoSQLDatabaseClientProviderInterface {
 public:
  AwsDynamoDBClientProvider(
      std::shared_ptr<InstanceClientProviderInterface> instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      std::shared_ptr<DynamoDBFactory> dynamo_db_factory =
          std::make_shared<DynamoDBFactory>())
      : instance_client_(instance_client),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        dynamo_db_factory_(dynamo_db_factory) {}

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
  /// Creates ClientConfig to create DynamoDbClient.
  core::ExecutionResultOr<Aws::Client::ClientConfiguration>
  CreateClientConfig() noexcept;

  /**
   * @brief Is called when the response of query item request is ready.
   *
   * @param get_database_item_context The context object of the get database
   * item operation.
   * @param dynamo_db_client An instance of the dynamo db client.
   * @param query_request The query request object.
   * @param outcome The outcome of the operation.
   * @param async_context The async context of the sender. This is not used
   * based on SCP architecture.
   */
  void OnGetDatabaseItemCallback(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context,
      const Aws::DynamoDB::DynamoDBClient* dynamo_db_client,
      const Aws::DynamoDB::Model::QueryRequest& query_request,
      const Aws::Utils::Outcome<Aws::DynamoDB::Model::QueryResult,
                                Aws::DynamoDB::DynamoDBError>& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the response of upsert item request is ready.
   *
   * @param upsert_database_item_context The context object of the upsert
   * database item operation.
   * @param dynamo_db_client An instance of the dynamo db client.
   * @param query_request The query request object.
   * @param outcome The outcome of the operation.
   * @param async_context The async context of the sender. This is not used
   * based on SCP architecture.
   */
  void OnUpsertDatabaseItemCallback(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context,
      const Aws::DynamoDB::DynamoDBClient* dynamo_db_client,
      const Aws::DynamoDB::Model::UpdateItemRequest& update_item_request,
      const Aws::Utils::Outcome<Aws::DynamoDB::Model::UpdateItemResult,
                                Aws::DynamoDB::DynamoDBError>& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /// Instance client.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_;

  /// An instance fo the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  std::shared_ptr<DynamoDBFactory> dynamo_db_factory_;

  /// An instance of the AWS dynamo db client.
  std::shared_ptr<Aws::DynamoDB::DynamoDBClient> dynamo_db_client_;
};

class DynamoDBFactory {
 public:
  virtual core::ExecutionResultOr<
      std::shared_ptr<Aws::DynamoDB::DynamoDBClient>>
  CreateClient(const Aws::Client::ClientConfiguration& client_config) noexcept;

  virtual ~DynamoDBFactory() = default;
};

}  // namespace google::scp::cpio::client_providers
