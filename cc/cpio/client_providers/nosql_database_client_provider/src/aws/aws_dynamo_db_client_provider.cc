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

#include "aws_dynamo_db_client_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>

#include "absl/strings/str_cat.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_utils.h"
#include "cpio/common/src/aws/aws_utils.h"

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
using google::cmrt::sdk::nosql_database_service::v1::CreateTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::CreateTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::Item;
using google::cmrt::sdk::nosql_database_service::v1::ItemAttribute;
using google::cmrt::sdk::nosql_database_service::v1::ItemKey;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND;
using google::scp::cpio::client_providers::AwsDynamoDBUtils;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using std::bind;
using std::make_shared;
using std::move;
using std::optional;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace {

constexpr size_t kExpressionsInitialByteSize = 1024;
constexpr char kDynamoDB[] = "DynamoDB";
constexpr size_t kMaxConcurrentConnections = 1000;

}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResultOr<ClientConfiguration>
AwsDynamoDBClientProvider::CreateClientConfig() noexcept {
  ClientConfiguration client_config;
  client_config.maxConnections = kMaxConcurrentConnections;
  client_config.executor = make_shared<AwsAsyncExecutor>(io_async_executor_);

  auto region_code_or =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client_);
  if (!region_code_or.Successful()) {
    SCP_ERROR(kDynamoDB, kZeroUuid, region_code_or.result(),
              "Failed to get region code for current instance");
    return region_code_or.result();
  }
  client_config.region = (*region_code_or).c_str();

  return client_config;
}

ExecutionResult AwsDynamoDBClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsDynamoDBClientProvider::Run() noexcept {
  auto config_or = CreateClientConfig();
  if (!config_or.Successful()) {
    SCP_ERROR(kDynamoDB, kZeroUuid, config_or.result(),
              "Error creating ClientConfig");
    return config_or.result();
  }

  auto client_or = dynamo_db_factory_->CreateClient(*config_or);
  if (!client_or.Successful()) {
    SCP_ERROR(kDynamoDB, kZeroUuid, client_or.result(),
              "Error creating DynamoDBClient");
    return client_or.result();
  }

  dynamo_db_client_ = move(*client_or);

  return SuccessExecutionResult();
}

ExecutionResult AwsDynamoDBClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsDynamoDBClientProvider::CreateTable(
    AsyncContext<CreateTableRequest, CreateTableResponse>&
        create_table_context) noexcept {
  // TODO implement
  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult AwsDynamoDBClientProvider::DeleteTable(
    AsyncContext<DeleteTableRequest, DeleteTableResponse>&
        delete_table_context) noexcept {
  // TODO implement
  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult AwsDynamoDBClientProvider::GetDatabaseItem(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  const auto& request = *get_database_item_context.request;
  QueryRequest get_item_request;

  // Set the table name
  const auto& request_table_name = request.key().table_name();
  String table_name(request_table_name.c_str(), request_table_name.size());
  get_item_request.SetTableName(table_name);

  const auto key_container_or =
      AwsDynamoDBUtils::GetPartitionAndSortKeyValues(get_database_item_context);
  if (!key_container_or.Successful()) {
    get_database_item_context.result = key_container_or.result();
    get_database_item_context.Finish();
    return key_container_or.result();
  }

  // Set the condition expression
  Map<String, AttributeValue> attribute_values;
  String condition_expression;
  condition_expression.reserve(kExpressionsInitialByteSize);
  condition_expression =
      absl::StrCat(key_container_or->partition_key_name, "= :partition_key");
  attribute_values.emplace(":partition_key",
                           move(key_container_or->partition_key_val));

  // Sort key is optional
  if (key_container_or->sort_key_name.has_value()) {
    condition_expression =
        absl::StrCat(condition_expression, " and ",
                     *key_container_or->sort_key_name, "= :sort_key");
    attribute_values.emplace(":sort_key",
                             move(*key_container_or->sort_key_val));
  }
  get_item_request.SetKeyConditionExpression(condition_expression);

  // Set the filter expression
  if (!request.required_attributes().empty()) {
    const auto filter_expression_or =
        AwsDynamoDBUtils::GetConditionExpressionAndAddValuesToMap(
            get_database_item_context, attribute_values);
    if (!filter_expression_or.Successful()) {
      get_database_item_context.result = filter_expression_or.result();
      get_database_item_context.Finish();
      return filter_expression_or.result();
    }
    get_item_request.SetFilterExpression(move(*filter_expression_or));
  }
  get_item_request.SetExpressionAttributeValues(move(attribute_values));

  dynamo_db_client_->QueryAsync(
      get_item_request,
      bind(&AwsDynamoDBClientProvider::OnGetDatabaseItemCallback, this,
           get_database_item_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsDynamoDBClientProvider::OnGetDatabaseItemCallback(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context,
    const DynamoDBClient* dynamo_db_client, const QueryRequest& query_request,
    const Outcome<QueryResult, DynamoDBError>& outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!outcome.IsSuccess()) {
    auto result = AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
        outcome.GetError().GetErrorType());
    SCP_ERROR_CONTEXT(kDynamoDB, get_database_item_context, result,
                      "Get database item request failed. Error code: %d, "
                      "message: %s",
                      outcome.GetError().GetResponseCode(),
                      outcome.GetError().GetMessage().c_str());
    FinishContext(result, get_database_item_context, cpu_async_executor_);
    return;
  }
  const auto& request = *get_database_item_context.request;
  const auto& request_table_name = request.key().table_name();

  auto items = outcome.GetResult().GetItems();
  if (items.size() != 1) {
    auto result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kDynamoDB, get_database_item_context, result,
        "Found %d items when we were looking for exactly 1 in table %s",
        items.size(), request_table_name.c_str())
    FinishContext(result, get_database_item_context, cpu_async_executor_);
    return;
  }
  const auto& item = items[0];
  const String partition_key_name(request.key().partition_key().name().c_str(),
                                  request.key().partition_key().name().size());

  get_database_item_context.response = make_shared<GetDatabaseItemResponse>();
  get_database_item_context.response->mutable_item()->mutable_key()->CopyFrom(
      request.key());

  for (const auto& attribute_key_value_pair : item) {
    // If the attribute is the partition or sort key, skip it.
    if (strcmp(attribute_key_value_pair.first.c_str(),
               request.key().partition_key().name().c_str()) == 0 ||
        (request.key().has_sort_key() &&
         strcmp(attribute_key_value_pair.first.c_str(),
                request.key().sort_key().name().c_str()) == 0)) {
      continue;
    }

    auto attribute_or = AwsDynamoDBUtils::ConvertDynamoDBTypeToItemAttribute(
        attribute_key_value_pair.second);

    if (!attribute_or.Successful()) {
      SCP_ERROR_CONTEXT(kDynamoDB, get_database_item_context,
                        attribute_or.result(),
                        "Error converting returned DynamoDB attribute to "
                        "ItemAttribute for table %s",
                        request_table_name.c_str());
      FinishContext(attribute_or.result(), get_database_item_context,
                    cpu_async_executor_);
      return;
    }

    attribute_or->set_name(attribute_key_value_pair.first.c_str());
    *get_database_item_context.response->mutable_item()->add_attributes() =
        move(*attribute_or);
  }

  FinishContext(SuccessExecutionResult(), get_database_item_context,
                cpu_async_executor_);
}

ExecutionResult AwsDynamoDBClientProvider::UpsertDatabaseItem(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  const auto& request = *upsert_database_item_context.request;
  UpdateItemRequest update_item_request;
  // Set the table name
  const String table_name(request.key().table_name().c_str(),
                          request.key().table_name().size());
  update_item_request.SetTableName(table_name);

  const auto key_container_or = AwsDynamoDBUtils::GetPartitionAndSortKeyValues(
      upsert_database_item_context);
  if (!key_container_or.Successful()) {
    upsert_database_item_context.result = key_container_or.result();
    upsert_database_item_context.Finish();
    return key_container_or.result();
  }

  // Set the partition key
  update_item_request.AddKey(move(key_container_or->partition_key_name),
                             move(key_container_or->partition_key_val));

  // Sort key is optional
  if (key_container_or->sort_key_name.has_value()) {
    // Set the sort key
    update_item_request.AddKey(move(*key_container_or->sort_key_name),
                               move(*key_container_or->sort_key_val));
  }

  // Set the update expression
  Map<String, AttributeValue> attribute_values;

  if (!request.new_attributes().empty()) {
    String update_expression;
    update_expression.reserve(kExpressionsInitialByteSize);
    update_expression += "SET ";

    for (size_t attribute_index = 0;
         attribute_index < request.new_attributes_size(); ++attribute_index) {
      update_expression +=
          absl::StrCat(request.new_attributes(attribute_index).name(),
                       "= :new_attribute_", attribute_index);

      auto attribute_value_or =
          AwsDynamoDBUtils::ConvertItemAttributeToDynamoDBType(
              request.new_attributes(attribute_index));
      if (!attribute_value_or.Successful()) {
        upsert_database_item_context.result = attribute_value_or.result();
        SCP_ERROR_CONTEXT(kDynamoDB, upsert_database_item_context,
                          attribute_value_or.result(),
                          "Error converting ItemAttribute type for table %s",
                          request.key().table_name().c_str());
        upsert_database_item_context.Finish();
        return attribute_value_or.result();
      }

      attribute_values.emplace(absl::StrCat(":new_attribute_", attribute_index),
                               move(*attribute_value_or));

      if (attribute_index + 1 < request.new_attributes_size()) {
        update_expression += " , ";
      }
    }
    update_item_request.SetUpdateExpression(update_expression);
  }

  // Set the condition expression
  if (!request.required_attributes().empty()) {
    const auto condition_expression_or =
        AwsDynamoDBUtils::GetConditionExpressionAndAddValuesToMap(
            upsert_database_item_context, attribute_values);
    if (!condition_expression_or.Successful()) {
      upsert_database_item_context.result = condition_expression_or.result();
      upsert_database_item_context.Finish();
      return condition_expression_or.result();
    }
    update_item_request.SetConditionExpression(move(*condition_expression_or));
  }

  update_item_request.SetExpressionAttributeValues(attribute_values);

  dynamo_db_client_->UpdateItemAsync(
      update_item_request,
      bind(&AwsDynamoDBClientProvider::OnUpsertDatabaseItemCallback, this,
           upsert_database_item_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsDynamoDBClientProvider::OnUpsertDatabaseItemCallback(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context,
    const DynamoDBClient* dynamo_db_client,
    const UpdateItemRequest& update_item_request,
    const Outcome<UpdateItemResult, DynamoDBError>& outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  ExecutionResult result = SuccessExecutionResult();
  if (!outcome.IsSuccess()) {
    result = AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
        outcome.GetError().GetErrorType());
    SCP_ERROR_CONTEXT(kDynamoDB, upsert_database_item_context, result,
                      "Upsert database item request failed. Error code: %d, "
                      "message: %s",
                      outcome.GetError().GetResponseCode(),
                      outcome.GetError().GetMessage().c_str());
  }

  FinishContext(result, upsert_database_item_context, cpu_async_executor_);
}

ExecutionResultOr<shared_ptr<DynamoDBClient>> DynamoDBFactory::CreateClient(
    const ClientConfiguration& client_config) noexcept {
  return make_shared<DynamoDBClient>(client_config);
}

shared_ptr<NoSQLDatabaseClientProviderInterface>
NoSQLDatabaseClientProviderFactory::Create(
    const shared_ptr<NoSQLDatabaseClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<AwsDynamoDBClientProvider>(
      instance_client, cpu_async_executor, io_async_executor);
}

}  // namespace google::scp::cpio::client_providers
