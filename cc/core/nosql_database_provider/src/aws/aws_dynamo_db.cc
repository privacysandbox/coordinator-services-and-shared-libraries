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

#include "aws_dynamo_db.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>

#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/interface/configuration_keys.h"
#include "core/nosql_database_provider/src/common/nosql_database_provider_utils.h"

#include "aws_dynamo_db_utils.h"

using Aws::Map;
using Aws::String;
using Aws::Client::AsyncCallerContext;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using Aws::DynamoDB::DynamoDBClient;
using Aws::DynamoDB::DynamoDBError;
using Aws::DynamoDB::Model::AttributeValue;
using Aws::DynamoDB::Model::QueryRequest;
using Aws::DynamoDB::Model::QueryResult;
using Aws::DynamoDB::Model::UpdateItemRequest;
using Aws::DynamoDB::Model::UpdateItemResult;
using Aws::Utils::Outcome;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::nosql_database_provider::AwsDynamoDBUtils;
using google::scp::core::nosql_database_provider::NoSQLDatabaseProviderUtils;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

static constexpr size_t kExpressionsInitialByteSize = 1024;
static constexpr char kDynamoDB[] = "DynamoDB";
static constexpr size_t kMaxConcurrentConnections = 1000;

namespace google::scp::core::nosql_database_provider {
ExecutionResult AwsDynamoDB::CreateClientConfig() noexcept {
  client_config_ = make_shared<ClientConfiguration>();
  client_config_->maxConnections = kMaxConcurrentConnections;
  client_config_->executor = make_shared<AwsAsyncExecutor>(io_async_executor_);

  string region;
  auto execution_result = config_provider_->Get(kCloudServiceRegion, region);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  String aws_region(region);
  client_config_->region = aws_region;

  return SuccessExecutionResult();
}

ExecutionResult AwsDynamoDB::Init() noexcept {
  auto execution_result = CreateClientConfig();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  dynamo_db_client_ = make_shared<DynamoDBClient>(*client_config_);

  return SuccessExecutionResult();
}

ExecutionResult AwsDynamoDB::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsDynamoDB::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsDynamoDB::GetDatabaseItem(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  QueryRequest get_item_request;

  // Set the table name
  const String table_name(*get_database_item_context.request->table_name);
  get_item_request.SetTableName(table_name);

  // Set the partition key
  const String partition_key_name(
      *get_database_item_context.request->partition_key->attribute_name);
  AttributeValue partition_key_value;
  auto execution_result = AwsDynamoDBUtils::
      ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
          *get_database_item_context.request->partition_key->attribute_value,
          partition_key_value);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Set the condition expression
  Map<String, AttributeValue> attribute_values;
  String condition_expression;
  condition_expression.reserve(kExpressionsInitialByteSize);
  condition_expression = partition_key_name + "= :partition_key";
  attribute_values.emplace(":partition_key", partition_key_value);

  // Sort key is optional
  if (get_database_item_context.request->sort_key) {
    // Set the sort key
    const String sort_key_name(
        *get_database_item_context.request->sort_key->attribute_name);
    AttributeValue sort_key_value;
    execution_result = AwsDynamoDBUtils::
        ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
            *get_database_item_context.request->sort_key->attribute_value,
            sort_key_value);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    condition_expression =
        condition_expression + " and " + sort_key_name + "= :sort_key";
    attribute_values.emplace(":sort_key", sort_key_value);
  }
  get_item_request.SetKeyConditionExpression(condition_expression);

  // Set the filter expression
  if (get_database_item_context.request->attributes &&
      get_database_item_context.request->attributes->size() > 0) {
    String filter_expression;
    filter_expression.reserve(kExpressionsInitialByteSize);

    for (size_t attribute_index = 0;
         attribute_index <
         get_database_item_context.request->attributes->size();
         ++attribute_index) {
      filter_expression +=
          *get_database_item_context.request->attributes->at(attribute_index)
               .attribute_name +
          "= :attribute_" + std::to_string(attribute_index);

      AttributeValue attribute_value;
      execution_result = AwsDynamoDBUtils::
          ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
              *get_database_item_context.request->attributes
                   ->at(attribute_index)
                   .attribute_value,
              attribute_value);
      if (!execution_result.Successful()) {
        return execution_result;
      }

      attribute_values.emplace(":attribute_" + std::to_string(attribute_index),
                               attribute_value);

      if (attribute_index + 1 <
          get_database_item_context.request->attributes->size()) {
        filter_expression += " and ";
      }
    }
    get_item_request.SetFilterExpression(filter_expression);
  }
  get_item_request.SetExpressionAttributeValues(attribute_values);

  dynamo_db_client_->QueryAsync(
      get_item_request,
      bind(&AwsDynamoDB::OnGetDatabaseItemCallback, this,
           get_database_item_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsDynamoDB::OnGetDatabaseItemCallback(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context,
    const DynamoDBClient* dynamo_db_client, const QueryRequest& query_request,
    const Outcome<QueryResult, DynamoDBError>& outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!outcome.IsSuccess()) {
    SCP_DEBUG_CONTEXT(
        kDynamoDB, get_database_item_context,
        "DynamoDB get database item request failed. Error code: %d, "
        "message: %s",
        outcome.GetError().GetResponseCode(),
        outcome.GetError().GetMessage().c_str());
    get_database_item_context.result =
        AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
            outcome.GetError().GetErrorType());
    if (!async_executor_
             ->Schedule(
                 [get_database_item_context]() mutable {
                   get_database_item_context.Finish();
                 },
                 AsyncPriority::High)
             .Successful()) {
      get_database_item_context.Finish();
    }
    return;
  }

  auto items = outcome.GetResult().GetItems();
  if (items.size() != 1) {
    get_database_item_context.result = FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
    if (!async_executor_
             ->Schedule(
                 [get_database_item_context]() mutable {
                   get_database_item_context.Finish();
                 },
                 AsyncPriority::High)
             .Successful()) {
      get_database_item_context.Finish();
    }
    return;
  }
  auto item = items[0];
  const String partition_key_name(
      *get_database_item_context.request->partition_key->attribute_name);

  get_database_item_context.response = make_shared<GetDatabaseItemResponse>();
  get_database_item_context.response->table_name =
      get_database_item_context.request->table_name;
  get_database_item_context.response->partition_key =
      get_database_item_context.request->partition_key;
  get_database_item_context.response->sort_key =
      get_database_item_context.request->sort_key;
  get_database_item_context.response->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();

  for (const auto& attribute_key_value_pair : item) {
    if (strcmp(attribute_key_value_pair.first.c_str(),
               get_database_item_context.request->partition_key->attribute_name
                   ->c_str()) == 0 ||
        (get_database_item_context.request->sort_key &&
         strcmp(attribute_key_value_pair.first.c_str(),
                get_database_item_context.request->sort_key->attribute_name
                    ->c_str()) == 0)) {
      continue;
    }

    NoSQLDatabaseValidAttributeValueTypes attribute_value;
    auto execution_result = AwsDynamoDBUtils::
        ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueType(
            attribute_key_value_pair.second, attribute_value);

    if (!execution_result.Successful()) {
      get_database_item_context.result = execution_result;
      if (!async_executor_
               ->Schedule(
                   [get_database_item_context]() mutable {
                     get_database_item_context.Finish();
                   },
                   AsyncPriority::High)
               .Successful()) {
        get_database_item_context.Finish();
      }
      return;
    }

    NoSqlDatabaseKeyValuePair key_value_pair;
    key_value_pair.attribute_name =
        make_shared<string>(attribute_key_value_pair.first.c_str());
    key_value_pair.attribute_value =
        make_shared<NoSQLDatabaseValidAttributeValueTypes>(
            move(attribute_value));
    get_database_item_context.response->attributes->push_back(key_value_pair);
  }

  get_database_item_context.result = SuccessExecutionResult();
  if (!async_executor_
           ->Schedule(
               [get_database_item_context]() mutable {
                 get_database_item_context.Finish();
               },
               AsyncPriority::High)
           .Successful()) {
    get_database_item_context.Finish();
  }
}

ExecutionResult AwsDynamoDB::UpsertDatabaseItem(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  UpdateItemRequest update_item_request;
  // Set the table name
  const String table_name(*upsert_database_item_context.request->table_name);
  update_item_request.SetTableName(table_name);

  // Set the partition key
  const String partition_key_name(
      *upsert_database_item_context.request->partition_key->attribute_name);
  AttributeValue partition_key_value;
  auto execution_result = AwsDynamoDBUtils::
      ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
          *upsert_database_item_context.request->partition_key->attribute_value,
          partition_key_value);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  update_item_request.AddKey(partition_key_name, partition_key_value);

  // Sort key is optional
  if (upsert_database_item_context.request->sort_key) {
    // Set the sort key
    const String sort_key_name(
        *upsert_database_item_context.request->sort_key->attribute_name);
    AttributeValue sort_key_value;
    execution_result = AwsDynamoDBUtils::
        ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
            *upsert_database_item_context.request->sort_key->attribute_value,
            sort_key_value);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    update_item_request.AddKey(sort_key_name, sort_key_value);
  }

  // Set the update expression
  Map<String, AttributeValue> attribute_values;

  if (upsert_database_item_context.request->new_attributes &&
      upsert_database_item_context.request->new_attributes->size() > 0) {
    String update_expression;
    update_expression.reserve(kExpressionsInitialByteSize);
    update_expression += "SET ";

    for (size_t attribute_index = 0;
         attribute_index <
         upsert_database_item_context.request->new_attributes->size();
         ++attribute_index) {
      update_expression += *upsert_database_item_context.request->new_attributes
                                ->at(attribute_index)
                                .attribute_name +
                           "= :new_attribute_" +
                           std::to_string(attribute_index);

      AttributeValue attribute_value;
      execution_result = AwsDynamoDBUtils::
          ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
              *upsert_database_item_context.request->new_attributes
                   ->at(attribute_index)
                   .attribute_value,
              attribute_value);
      if (!execution_result.Successful()) {
        return execution_result;
      }

      attribute_values.emplace(
          ":new_attribute_" + std::to_string(attribute_index), attribute_value);

      if (attribute_index + 1 <
          upsert_database_item_context.request->new_attributes->size()) {
        update_expression += " , ";
      }
    }
    update_item_request.SetUpdateExpression(update_expression);
  }

  // Set the condition expression
  if (upsert_database_item_context.request->attributes &&
      upsert_database_item_context.request->attributes->size() > 0) {
    String condition_expression;
    condition_expression.reserve(kExpressionsInitialByteSize);

    for (size_t attribute_index = 0;
         attribute_index <
         upsert_database_item_context.request->attributes->size();
         ++attribute_index) {
      condition_expression +=
          *upsert_database_item_context.request->attributes->at(attribute_index)
               .attribute_name +
          "= :attribute_" + std::to_string(attribute_index);

      AttributeValue attribute_value;
      execution_result = AwsDynamoDBUtils::
          ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
              *upsert_database_item_context.request->attributes
                   ->at(attribute_index)
                   .attribute_value,
              attribute_value);
      if (!execution_result.Successful()) {
        return execution_result;
      }

      attribute_values.emplace(":attribute_" + std::to_string(attribute_index),
                               attribute_value);

      if (attribute_index + 1 <
          upsert_database_item_context.request->attributes->size()) {
        condition_expression += " and ";
      }
    }
    update_item_request.SetConditionExpression(condition_expression);
  }

  update_item_request.SetExpressionAttributeValues(attribute_values);

  dynamo_db_client_->UpdateItemAsync(
      update_item_request,
      bind(&AwsDynamoDB::OnUpsertDatabaseItemCallback, this,
           upsert_database_item_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsDynamoDB::OnUpsertDatabaseItemCallback(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context,
    const DynamoDBClient* dynamo_db_client,
    const UpdateItemRequest& update_item_request,
    const Outcome<UpdateItemResult, DynamoDBError>& outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!outcome.IsSuccess()) {
    SCP_DEBUG_CONTEXT(
        kDynamoDB, upsert_database_item_context,
        "DynamoDB upsert database item request failed. Error code: %d, "
        "message: %s",
        outcome.GetError().GetResponseCode(),
        outcome.GetError().GetMessage().c_str());

    upsert_database_item_context.result =
        AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
            outcome.GetError().GetErrorType());
    if (!async_executor_
             ->Schedule(
                 [upsert_database_item_context]() mutable {
                   upsert_database_item_context.Finish();
                 },
                 AsyncPriority::High)
             .Successful()) {
      upsert_database_item_context.Finish();
    }
    return;
  }

  upsert_database_item_context.result = SuccessExecutionResult();
  if (!async_executor_
           ->Schedule(
               [upsert_database_item_context]() mutable {
                 upsert_database_item_context.Finish();
               },
               AsyncPriority::High)
           .Successful()) {
    upsert_database_item_context.Finish();
  }
}
}  // namespace google::scp::core::nosql_database_provider
