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

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>

namespace google::scp::core::nosql_database_provider::aws::mock {
class MockAwsDynamoDBClient : public Aws::DynamoDB::DynamoDBClient {
 public:
  MockAwsDynamoDBClient(
      const Aws::Client::ClientConfiguration& clientConfiguration)
      : Aws::DynamoDB::DynamoDBClient(clientConfiguration) {}

  MockAwsDynamoDBClient() : Aws::DynamoDB::DynamoDBClient() {}

  std::function<void(
      const Aws::DynamoDB::Model::QueryRequest&,
      const Aws::DynamoDB::QueryResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      query_async_mock;

  std::function<void(
      const Aws::DynamoDB::Model::UpdateItemRequest&,
      const Aws::DynamoDB::UpdateItemResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      update_item_async_mock;

  void QueryAsync(const Aws::DynamoDB::Model::QueryRequest& request,
                  const Aws::DynamoDB::QueryResponseReceivedHandler& handler,
                  const std::shared_ptr<const Aws::Client::AsyncCallerContext>&
                      context = nullptr) const override {
    if (query_async_mock) {
      query_async_mock(request, handler, context);
      return;
    }

    DynamoDBClient::QueryAsync(request, handler, context);
  }

  void UpdateItemAsync(
      const Aws::DynamoDB::Model::UpdateItemRequest& request,
      const Aws::DynamoDB::UpdateItemResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (update_item_async_mock) {
      update_item_async_mock(request, handler, context);
      return;
    }

    DynamoDBClient::UpdateItemAsync(request, handler, context);
  }
};
}  // namespace google::scp::core::nosql_database_provider::aws::mock
