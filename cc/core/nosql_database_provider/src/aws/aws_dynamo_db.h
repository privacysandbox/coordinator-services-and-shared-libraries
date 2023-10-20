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
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/nosql_database_provider_utils.h"

namespace google::scp::core::nosql_database_provider {
/*! @copydoc NoSQLDatabaseProviderInterface
 */
class AwsDynamoDB : public NoSQLDatabaseProviderInterface {
 protected:
  explicit AwsDynamoDB(
      std::shared_ptr<Aws::DynamoDB::DynamoDBClient> dynamo_db_client,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor)
      : async_executor_(async_executor), dynamo_db_client_(dynamo_db_client) {}

 public:
  AwsDynamoDB(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : async_executor_(async_executor),
        io_async_executor_(io_async_executor),
        config_provider_(config_provider) {}

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
  /// Creates ClientConfig to create DynamoDbClient.
  virtual ExecutionResult CreateClientConfig() noexcept;

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
  virtual void OnGetDatabaseItemCallback(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
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
  virtual void OnUpsertDatabaseItemCallback(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context,
      const Aws::DynamoDB::DynamoDBClient* dynamo_db_client,
      const Aws::DynamoDB::Model::UpdateItemRequest& update_item_request,
      const Aws::Utils::Outcome<Aws::DynamoDB::Model::UpdateItemResult,
                                Aws::DynamoDB::DynamoDBError>& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /// An instance fo the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance fo the IO async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;

  /// An instance of the AWS client configuration.
  std::shared_ptr<Aws::Client::ClientConfiguration> client_config_;

  /// An instance of the AWS dynamo db client.
  std::shared_ptr<Aws::DynamoDB::DynamoDBClient> dynamo_db_client_;

  /// An instance of the config provider.
  const std::shared_ptr<core::ConfigProviderInterface> config_provider_;
};
}  // namespace google::scp::core::nosql_database_provider
