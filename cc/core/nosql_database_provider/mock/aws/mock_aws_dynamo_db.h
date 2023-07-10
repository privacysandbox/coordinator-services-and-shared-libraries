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

#include <functional>
#include <memory>

#include "core/nosql_database_provider/mock/aws/mock_aws_dynamo_db_client.h"
#include "core/nosql_database_provider/src/aws/aws_dynamo_db.h"

namespace google::scp::core::nosql_database_provider::aws::mock {
class MockAwsDynamoDB : public nosql_database_provider::AwsDynamoDB {
 public:
  explicit MockAwsDynamoDB(
      std::shared_ptr<MockAwsDynamoDBClient> mock_dynamo_db_client,
      std::shared_ptr<core::AsyncExecutorInterface> async_executor)
      : AwsDynamoDB(std::dynamic_pointer_cast<Aws::DynamoDB::DynamoDBClient>(
                        mock_dynamo_db_client),
                    async_executor) {}

  std::function<ExecutionResult(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&)>
      get_database_item_mock;

  std::function<ExecutionResult(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&)>
      upsert_database_item_mock;

  std::function<void(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&,
      const Aws::DynamoDB::DynamoDBClient*,
      const Aws::DynamoDB::Model::QueryRequest&,
      const Aws::Utils::Outcome<Aws::DynamoDB::Model::QueryResult,
                                Aws::DynamoDB::DynamoDBError>&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>)>
      on_get_database_item_callback_mock;

  std::function<void(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&,
      const Aws::DynamoDB::DynamoDBClient*,
      const Aws::DynamoDB::Model::UpdateItemRequest&,
      const Aws::Utils::Outcome<Aws::DynamoDB::Model::UpdateItemResult,
                                Aws::DynamoDB::DynamoDBError>&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>)>
      on_upsert_database_item_callback_mock;

  ExecutionResult GetDatabaseItem(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
          get_database_item_context) noexcept override {
    if (get_database_item_mock) {
      return get_database_item_mock(get_database_item_context);
    }

    return AwsDynamoDB::GetDatabaseItem(get_database_item_context);
  }

  ExecutionResult UpsertDatabaseItem(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept override {
    if (upsert_database_item_mock) {
      return upsert_database_item_mock(upsert_database_item_context);
    }

    return AwsDynamoDB::UpsertDatabaseItem(upsert_database_item_context);
  }

  void OnGetDatabaseItemCallback(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
          get_database_item_context,
      const Aws::DynamoDB::DynamoDBClient* dynamo_db_client,
      const Aws::DynamoDB::Model::QueryRequest& query_request,
      const Aws::Utils::Outcome<Aws::DynamoDB::Model::QueryResult,
                                Aws::DynamoDB::DynamoDBError>& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    if (on_get_database_item_callback_mock) {
      on_get_database_item_callback_mock(get_database_item_context,
                                         dynamo_db_client, query_request,
                                         outcome, async_context);
      return;
    }

    AwsDynamoDB::OnGetDatabaseItemCallback(get_database_item_context,
                                           dynamo_db_client, query_request,
                                           outcome, async_context);
  }

  void OnUpsertDatabaseItemCallback(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context,
      const Aws::DynamoDB::DynamoDBClient* dynamo_db_client,
      const Aws::DynamoDB::Model::UpdateItemRequest& update_item_request,
      const Aws::Utils::Outcome<Aws::DynamoDB::Model::UpdateItemResult,
                                Aws::DynamoDB::DynamoDBError>& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept override {
    if (on_upsert_database_item_callback_mock) {
      on_upsert_database_item_callback_mock(
          upsert_database_item_context, dynamo_db_client, update_item_request,
          outcome, async_context);
      return;
    }

    AwsDynamoDB::OnUpsertDatabaseItemCallback(
        upsert_database_item_context, dynamo_db_client, update_item_request,
        outcome, async_context);
  }
};
}  // namespace google::scp::core::nosql_database_provider::aws::mock
