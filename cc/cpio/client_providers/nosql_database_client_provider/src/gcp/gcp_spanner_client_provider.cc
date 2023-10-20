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

#include "gcp_spanner_client_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

#include "gcp_spanner_utils.h"

using google::cloud::StatusOr;
using google::cloud::spanner::Client;
using google::cloud::spanner::Database;
using google::cloud::spanner::MakeConnection;
using google::cloud::spanner::MakeInsertOrUpdateMutation;
using google::cloud::spanner::Mutation;
using google::cloud::spanner::Mutations;
using google::cloud::spanner::RowStream;
using google::cloud::spanner::SqlStatement;
using google::cloud::spanner::Transaction;
using google::cloud::spanner::Value;
using google::cloud::spanner_admin::DatabaseAdminClient;
using google::cloud::spanner_admin::MakeDatabaseAdminConnection;
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
using google::protobuf::RepeatedPtrField;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_GCP_FAILED_PRECONDITION;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_EMPTY_TABLE_NAME;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_JSON_FAILED_TO_PARSE;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_CORRUPTED;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_TABLE_NOT_FOUND;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_UNSET_KEY_TYPE;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::client_providers::GcpSpannerUtils;
using google::scp::cpio::client_providers::PartitionAndSortKey;
using google::scp::cpio::common::GcpUtils;
using google::spanner::admin::database::v1::UpdateDatabaseDdlRequest;
using std::bind;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::move;
using std::optional;
using std::pair;
using std::ref;
using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;
using std::placeholders::_1;

using SpannerJson = google::cloud::spanner::Json;
using json = nlohmann::json;
using AttributesList = RepeatedPtrField<ItemAttribute>;

namespace {

constexpr char kGcpSpanner[] = "GcpSpanner";

constexpr char kValueColumnName[] = "Value";

constexpr char kPartitionKeyParamName[] = "partition_key";
constexpr char kSortKeyParamName[] = "sort_key";

constexpr int kValueColumnIndex = 0;

ExecutionResult ValidateCreateTableRequest(
    AsyncContext<CreateTableRequest, CreateTableResponse>&
        create_table_context) {
  if (create_table_context.request->key().table_name().empty()) {
    auto result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_EMPTY_TABLE_NAME);
    SCP_ERROR_CONTEXT(kGcpSpanner, create_table_context, result,
                      "Cannot create a table without a name");
    create_table_context.result = result;
    create_table_context.Finish();
    return result;
  }

  if (create_table_context.request->key().partition_key().name().empty()) {
    auto result = FailureExecutionResult(
        SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME);
    SCP_ERROR_CONTEXT(kGcpSpanner, create_table_context, result,
                      "Cannot create a table without a partition key name");
    create_table_context.result = result;
    create_table_context.Finish();
    return result;
  }

  if (create_table_context.request->key().partition_key().value_case() ==
      ItemAttribute::VALUE_NOT_SET) {
    auto result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_UNSET_KEY_TYPE);
    SCP_ERROR_CONTEXT(kGcpSpanner, create_table_context, result,
                      "Cannot create a table without a partition key type");
    create_table_context.result = result;
    create_table_context.Finish();
    return result;
  }

  if (create_table_context.request->key().has_sort_key()) {
    if (create_table_context.request->key().sort_key().name().empty()) {
      auto result = FailureExecutionResult(
          SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME);
      SCP_ERROR_CONTEXT(kGcpSpanner, create_table_context, result,
                        "Cannot create a table without a sort key name");
      create_table_context.result = result;
      create_table_context.Finish();
      return result;
    }

    if (create_table_context.request->key().sort_key().value_case() ==
        ItemAttribute::VALUE_NOT_SET) {
      auto result =
          FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_UNSET_KEY_TYPE);
      SCP_ERROR_CONTEXT(kGcpSpanner, create_table_context, result,
                        "Cannot create a table without a sort key type");
      create_table_context.result = result;
      create_table_context.Finish();
      return result;
    }
  }
  return SuccessExecutionResult();
}

// Builds a CREATE TABLE string to imitate a NoSQL table given request.
string BuildCreateTableStatement(const CreateTableRequest& request) {
  string str;
  if (request.key().has_sort_key()) {
    str = absl::Substitute(
        R"(
          CREATE TABLE $0 (
            $1 $2 NOT NULL,
            $3 $4 NOT NULL,
            Value JSON
          ) PRIMARY KEY($1, $3)
        )",
        request.key().table_name(), request.key().partition_key().name(),
        GcpSpannerUtils::ConvertAttributeTypeToSpannerTypeName(
            request.key().partition_key()),
        request.key().sort_key().name(),
        GcpSpannerUtils::ConvertAttributeTypeToSpannerTypeName(
            request.key().sort_key()));
  } else {
    str = absl::Substitute(
        R"(
          CREATE TABLE $0 (
            $1 $2 NOT NULL,
            Value JSON
          ) PRIMARY KEY($1)
        )",
        request.key().table_name(), request.key().partition_key().name(),
        GcpSpannerUtils::ConvertAttributeTypeToSpannerTypeName(
            request.key().partition_key()));
  }
  absl::RemoveExtraAsciiWhitespace(&str);
  return str;
}

// Returns success if the partition and sort key in req match the stored
// values in table_name_to_keys. Returns success if table_name_to_keys is
// not present.
ExecutionResult ValidatePartitionAndSortKey(
    const unordered_map<string, PartitionAndSortKey>* table_name_to_keys,
    const ItemKey& key) {
  if (!table_name_to_keys) return SuccessExecutionResult();

  auto it = table_name_to_keys->find(key.table_name());
  if (it == table_name_to_keys->end()) {
    return FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_TABLE_NOT_FOUND);
  }
  const auto& [partition_key, sort_key_expected] = it->second;
  if (key.partition_key().name() != partition_key) {
    return FailureExecutionResult(
        SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME);
  }

  bool has_non_empty_sort_key =
      key.has_sort_key() && !key.sort_key().name().empty();
  if (!sort_key_expected && has_non_empty_sort_key) {
    return FailureExecutionResult(
        SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME);
  }
  if (sort_key_expected && !has_non_empty_sort_key) {
    return FailureExecutionResult(
        SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME);
  }
  return SuccessExecutionResult();
}

// Given attributes, adds a condition to out to match a member in the Value
// column to the attribute. Also adds these parameters to params.
// All members of attributes are assumed to be nested inside of the Value
// JSON column.
// Add filters of the form:
// "JSON_VALUE(Value, '$.token_count') = @attribute_0"
ExecutionResult AppendJsonWhereClauses(const AttributesList& attributes,
                                       SqlStatement::ParamType& params,
                                       string& out) {
  if (attributes.size() > 0) {
    for (size_t attribute_index = 0; attribute_index < attributes.size();
         ++attribute_index) {
      auto attribute_alias = absl::StrCat("attribute_", attribute_index);
      absl::StrAppend(&out,
                      absl::StrFormat(" AND JSON_VALUE(Value, '$.%s') = @%s",
                                      attributes.at(attribute_index).name(),
                                      attribute_alias));

      auto spanner_val_or = GcpSpannerUtils::ConvertItemAttributeToSpannerValue(
          attributes.at(attribute_index));
      RETURN_IF_FAILURE(spanner_val_or.result());
      params.emplace(attribute_alias, move(*spanner_val_or));
    }
  }
  return SuccessExecutionResult();
}

}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult GcpSpannerClientProvider::Init() noexcept {
  if (cpu_async_executor_ == nullptr) {
    auto result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR);
    SCP_ERROR(kGcpSpanner, kZeroUuid, result, "cpu_async_executor_ is null");
    return result;
  }
  if (io_async_executor_ == nullptr) {
    auto result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR);
    SCP_ERROR(kGcpSpanner, kZeroUuid, result, "io_async_executor_ is null");
    return result;
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpSpannerClientProvider::Run() noexcept {
  auto project_id_or =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client_);
  if (!project_id_or.Successful()) {
    SCP_ERROR(kGcpSpanner, kZeroUuid, project_id_or.result(),
              "Failed to get project ID for current instance");
    return project_id_or.result();
  }

  fully_qualified_db_name_ = absl::StrFormat(
      "projects/%s/instances/%s/databases/%s", *project_id_or,
      client_options_->instance_name, client_options_->database_name);

  auto client_or = spanner_factory_->CreateClients(
      *project_id_or, client_options_->instance_name,
      client_options_->database_name);
  if (!client_or.Successful()) {
    SCP_ERROR(kGcpSpanner, kZeroUuid, client_or.result(),
              "Failed creating Spanner clients");
    return client_or.result();
  }
  spanner_client_shared_ = client_or->first;
  spanner_database_client_shared_ = client_or->second;

  return SuccessExecutionResult();
}

ExecutionResult GcpSpannerClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void GcpSpannerClientProvider::CreateTableAsync(
    AsyncContext<CreateTableRequest, CreateTableResponse>&
        create_table_context) noexcept {
  DatabaseAdminClient spanner_database_client(*spanner_database_client_shared_);

  ExecutionResult result = SuccessExecutionResult();
  UpdateDatabaseDdlRequest request;
  request.set_database(fully_qualified_db_name_);
  request.add_statements(
      BuildCreateTableStatement(*create_table_context.request));
  auto update_result = spanner_database_client.UpdateDatabaseDdl(request).get();
  if (!update_result.ok()) {
    result = GcpUtils::GcpErrorConverter(update_result.status());
    SCP_ERROR_CONTEXT(kGcpSpanner, create_table_context, result,
                      "Error creating table %s. Error: %s",
                      create_table_context.request->key().table_name().c_str(),
                      update_result.status().message().c_str());
  }
  FinishContext(result, create_table_context, cpu_async_executor_);
}

ExecutionResult GcpSpannerClientProvider::CreateTable(
    AsyncContext<CreateTableRequest, CreateTableResponse>&
        create_table_context) noexcept {
  RETURN_IF_FAILURE(ValidateCreateTableRequest(create_table_context));

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpSpannerClientProvider::CreateTableAsync, this,
               create_table_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpSpanner, create_table_context, schedule_result,
                      "Error scheduling CreateTable for table %s",
                      create_table_context.request->key().table_name().c_str());
    create_table_context.result = schedule_result;
    create_table_context.Finish();
    return schedule_result;
  }

  return SuccessExecutionResult();
}

void GcpSpannerClientProvider::DeleteTableAsync(
    AsyncContext<DeleteTableRequest, DeleteTableResponse>&
        delete_table_context) noexcept {
  DatabaseAdminClient spanner_database_client(*spanner_database_client_shared_);

  ExecutionResult result = SuccessExecutionResult();
  UpdateDatabaseDdlRequest request;
  request.set_database(fully_qualified_db_name_);
  request.add_statements(
      absl::StrCat("DROP TABLE ", delete_table_context.request->table_name()));
  auto update_result = spanner_database_client.UpdateDatabaseDdl(request).get();
  if (!update_result.ok()) {
    result = GcpUtils::GcpErrorConverter(update_result.status());
    SCP_ERROR_CONTEXT(kGcpSpanner, delete_table_context, result,
                      "Error deleting table %s. Error: %s",
                      delete_table_context.request->table_name().c_str(),
                      update_result.status().message().c_str());
  }
  FinishContext(result, delete_table_context, cpu_async_executor_);
}

ExecutionResult GcpSpannerClientProvider::DeleteTable(
    AsyncContext<DeleteTableRequest, DeleteTableResponse>&
        delete_table_context) noexcept {
  if (delete_table_context.request->table_name().empty()) {
    auto result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_EMPTY_TABLE_NAME);
    SCP_ERROR_CONTEXT(kGcpSpanner, delete_table_context, result,
                      "Cannot delete a table without a name");
    delete_table_context.result = result;
    delete_table_context.Finish();
    return result;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpSpannerClientProvider::DeleteTableAsync, this,
               delete_table_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpSpanner, delete_table_context, schedule_result,
                      "Error scheduling DeleteTable for table %s",
                      delete_table_context.request->table_name().c_str());
    delete_table_context.result = schedule_result;
    delete_table_context.Finish();
    return schedule_result;
  }

  return SuccessExecutionResult();
}

void GcpSpannerClientProvider::GetDatabaseItemAsync(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
        get_database_item_context,
    string query, SqlStatement::ParamType params) noexcept {
  Client spanner_client(*spanner_client_shared_);
  auto row_stream =
      spanner_client.ExecuteQuery(SqlStatement(move(query), move(params)));

  auto row_it = row_stream.begin();
  if (row_it == row_stream.end() || !row_it->ok()) {
    ExecutionResult result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
    if (!row_it->ok()) {
      result = GcpUtils::GcpErrorConverter(row_it->status());
      SCP_ERROR_CONTEXT(
          kGcpSpanner, get_database_item_context, result,
          "Spanner get database item request failed for Database %s Table %s",
          client_options_->database_name.c_str(),
          get_database_item_context.request->key().table_name().c_str());
    }
    FinishContext(result, get_database_item_context, cpu_async_executor_);
    return;
  }

  const auto spanner_json_or = row_it->value().get<SpannerJson>(0);
  if (!spanner_json_or.ok()) {
    auto result = GcpUtils::GcpErrorConverter(spanner_json_or.status());
    SCP_ERROR_CONTEXT(
        kGcpSpanner, get_database_item_context, result,
        "Spanner get JSON Value column failed for Database %s Table %s",
        client_options_->database_name.c_str(),
        get_database_item_context.request->key().table_name().c_str());
    FinishContext(result, get_database_item_context, cpu_async_executor_);
    return;
  }

  get_database_item_context.response = make_shared<GetDatabaseItemResponse>();
  get_database_item_context.response->mutable_item()->mutable_key()->CopyFrom(
      get_database_item_context.request->key());

  json value_json;
  try {
    value_json = json::parse(string(*spanner_json_or));
  } catch (...) {
    auto result = FailureExecutionResult(
        SC_NO_SQL_DATABASE_PROVIDER_JSON_FAILED_TO_PARSE);
    SCP_ERROR_CONTEXT(kGcpSpanner, get_database_item_context, result,
                      "Spanner parse JSON Value column failed.");
    FinishContext(result, get_database_item_context, cpu_async_executor_);
    return;
  }

  // Populate response attributes from all of the elements in the Value
  // column.
  for (auto& [json_attr_name, json_attr_value] : value_json.items()) {
    auto attribute_or =
        GcpSpannerUtils::ConvertJsonTypeToItemAttribute(json_attr_value);
    if (!attribute_or.Successful()) {
      // If conversion fails, it is likely a list, struct, or other
      // unsupported type. Continue without failing.
      SCP_ERROR_CONTEXT(kGcpSpanner, get_database_item_context,
                        attribute_or.result(), "JSON field failed conversion");
      continue;
    }
    attribute_or->set_name(json_attr_name);
    *get_database_item_context.response->mutable_item()->add_attributes() =
        move(*attribute_or);
  }

  FinishContext(SuccessExecutionResult(), get_database_item_context,
                cpu_async_executor_);
}

ExecutionResult GcpSpannerClientProvider::GetDatabaseItem(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  const auto& request = *get_database_item_context.request;
  if (auto execution_result = ValidatePartitionAndSortKey(
          client_options_->table_name_to_keys.get(), request.key());
      !execution_result.Successful()) {
    auto sort_key_str = request.key().has_sort_key()
                            ? request.key().sort_key().name().c_str()
                            : "<empty>";
    SCP_ERROR_CONTEXT(
        kGcpSpanner, get_database_item_context, execution_result,
        "Failed validating Partition key (%s) and Sort key (%s) for table %s",
        request.key().partition_key().name().c_str(), sort_key_str,
        request.key().table_name().c_str());
    get_database_item_context.result = execution_result;
    get_database_item_context.Finish();
    return execution_result;
  }
  // Build a query like:
  // SELECT IFNULL(Value, JSON '{}')
  // FROM `BudgetKeys`
  // WHERE BudgetKeyId = @partition_key
  //   AND Timeframe = @sort_key
  //   AND JSON_VALUE(Value, '$.token_count') = @attribute_0
  //   AND JSON_VALUE(Value, '$.grammy_awards') = @attribute_1

  // Set the table name
  const auto& table_name = request.key().table_name();
  string query =
      absl::StrFormat("SELECT IFNULL(%s, JSON '{}') FROM `%s` WHERE ",
                      kValueColumnName, table_name);

  // Set the partition key
  auto& provided_partition_key = request.key().partition_key();
  auto spanner_part_key_val_or =
      GcpSpannerUtils::ConvertItemAttributeToSpannerValue(
          provided_partition_key);
  if (!spanner_part_key_val_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpSpanner, get_database_item_context,
        spanner_part_key_val_or.result(),
        "Error converting provided partition key to Spanner value");
    get_database_item_context.result = spanner_part_key_val_or.result();
    get_database_item_context.Finish();
    return spanner_part_key_val_or.result();
  }
  absl::StrAppend(&query, provided_partition_key.name(), " = @",
                  kPartitionKeyParamName);

  SqlStatement::ParamType params;
  params.emplace(kPartitionKeyParamName, move(*spanner_part_key_val_or));

  // sort_key is optional
  if (request.key().has_sort_key()) {
    auto spanner_sort_key_val_or =
        GcpSpannerUtils::ConvertItemAttributeToSpannerValue(
            request.key().sort_key());
    if (!spanner_sort_key_val_or.Successful()) {
      SCP_ERROR_CONTEXT(kGcpSpanner, get_database_item_context,
                        spanner_sort_key_val_or.result(),
                        "Error converting provided sort key to Spanner value");
      get_database_item_context.result = spanner_sort_key_val_or.result();
      get_database_item_context.Finish();
      return spanner_sort_key_val_or.result();
    }

    absl::StrAppend(&query, " AND ", request.key().sort_key().name(), " = @",
                    kSortKeyParamName);
    params.emplace(kSortKeyParamName, move(*spanner_sort_key_val_or));
  }

  // Set the filter expression
  if (auto execution_result =
          AppendJsonWhereClauses(request.required_attributes(), params, query);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpSpanner, get_database_item_context, execution_result,
                      "Error appending JSON where clauses");
    get_database_item_context.result = execution_result;
    get_database_item_context.Finish();
    return execution_result;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpSpannerClientProvider::GetDatabaseItemAsync, this,
               get_database_item_context, move(query), move(params)),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpSpanner, get_database_item_context, schedule_result,
                      "Error scheduling GetDatabaseItem");
    get_database_item_context.result = schedule_result;
    get_database_item_context.Finish();
    return schedule_result;
  }

  return SuccessExecutionResult();
}

ExecutionResultOr<GcpSpannerClientProvider::UpsertSelectOptions>
GcpSpannerClientProvider::UpsertSelectOptions::BuildUpsertSelectOptions(
    const UpsertDatabaseItemRequest& request) {
  UpsertSelectOptions upsert_select_options;
  const auto& table_name = request.key().table_name();

  string select_query = absl::StrFormat("SELECT %s FROM `%s` WHERE ",
                                        kValueColumnName, table_name);
  SqlStatement::ParamType params;

  // Set the partition key
  auto partition_key_val_or =
      GcpSpannerUtils::ConvertItemAttributeToSpannerValue(
          request.key().partition_key());
  RETURN_IF_FAILURE(partition_key_val_or.result());
  upsert_select_options.partition_key_val = move(*partition_key_val_or);
  absl::StrAppendFormat(&select_query, "%s = @%s",
                        request.key().partition_key().name(),
                        kPartitionKeyParamName);
  params.emplace(kPartitionKeyParamName,
                 upsert_select_options.partition_key_val);
  upsert_select_options.column_names.push_back(
      request.key().partition_key().name());

  // sort_key is optional
  if (request.key().has_sort_key() &&
      !request.key().sort_key().name().empty()) {
    auto sort_key_val_or = GcpSpannerUtils::ConvertItemAttributeToSpannerValue(
        request.key().sort_key());
    RETURN_IF_FAILURE(sort_key_val_or.result());
    upsert_select_options.sort_key_val = move(*sort_key_val_or);
    absl::StrAppendFormat(&select_query, " AND %s = @%s",
                          request.key().sort_key().name(), kSortKeyParamName);
    params.emplace(kSortKeyParamName, upsert_select_options.sort_key_val);
    upsert_select_options.column_names.push_back(
        request.key().sort_key().name());
  }
  upsert_select_options.column_names.push_back(kValueColumnName);

  // Set the filter expression
  RETURN_IF_FAILURE(AppendJsonWhereClauses(request.required_attributes(),
                                           params, select_query));

  upsert_select_options.select_statement =
      SqlStatement(move(select_query), move(params));

  return upsert_select_options;
}

ExecutionResult GcpSpannerClientProvider::GetMergedJson(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context,
    RowStream& row_stream, bool enforce_row_existence, json new_attributes,
    optional<SpannerJson>& spanner_json) {
  auto row_it = row_stream.begin();
  if (!row_it->ok()) {
    auto result = GcpUtils::GcpErrorConverter(row_it->status());
    SCP_ERROR_CONTEXT(
        kGcpSpanner, upsert_database_item_context, result,
        "Spanner upsert SELECT statement failed for Database %s Table %s",
        client_options_->database_name.c_str(),
        upsert_database_item_context.request->key().table_name().c_str());
    return result;
  }
  if (row_it == row_stream.end()) {
    if (enforce_row_existence) {
      auto result =
          FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
      SCP_ERROR_CONTEXT(kGcpSpanner, upsert_database_item_context, result,
                        "Spanner update failed because row does not exist");
      return result;
    }
    // There's not existing row - that's OK, set spanner_json to have
    // new_attributes.
    if (!new_attributes.empty()) {
      spanner_json = SpannerJson(new_attributes.dump());
      return SuccessExecutionResult();
    }
  }

  json existing_json;

  const auto spanner_json_or =
      row_it->value().get<SpannerJson>(kValueColumnIndex);
  if (!spanner_json_or.ok()) {
    auto result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RECORD_CORRUPTED);
    SCP_ERROR_CONTEXT(
        kGcpSpanner, upsert_database_item_context, result,
        absl::StrFormat("Spanner get JSON Value column failed. Error code: %d, "
                        "message: %s",
                        spanner_json_or.status().code(),
                        spanner_json_or.status().message()));
    return result;
  }

  try {
    existing_json = json::parse(string(*spanner_json_or));
  } catch (...) {
    auto result = FailureExecutionResult(
        SC_NO_SQL_DATABASE_PROVIDER_JSON_FAILED_TO_PARSE);
    SCP_ERROR_CONTEXT(kGcpSpanner, upsert_database_item_context, result,
                      "Failed to parse JSON value from string");
    return result;
  }

  json final_json = move(new_attributes);
  // Emplace all members from the existing value only if they do not already
  // exist.
  for (const auto& [key, val] : existing_json.items()) {
    if (!final_json.contains(key)) {
      final_json[key] = val;
    }
  }

  if (!final_json.empty()) {
    spanner_json = SpannerJson(final_json.dump());
  }
  return SuccessExecutionResult();
}

Mutations GcpSpannerClientProvider::UpsertFunctor(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context,
    Client& client, const UpsertSelectOptions& upsert_select_options,
    bool enforce_row_existence, const nlohmann::json& new_attributes,
    ExecutionResult& prepare_result, const string& table_name,
    Transaction txn) {
  auto row_stream =
      client.ExecuteQuery(txn, upsert_select_options.select_statement);

  optional<SpannerJson> spanner_json;
  prepare_result =
      GetMergedJson(upsert_database_item_context, row_stream,
                    enforce_row_existence, move(new_attributes), spanner_json);
  if (!prepare_result.Successful()) {
    return Mutations{};
  }

  if (upsert_select_options.sort_key_val.get<string>().ok()) {
    // Include the sort_key column value.
    return Mutations{MakeInsertOrUpdateMutation(
        table_name, upsert_select_options.column_names,
        upsert_select_options.partition_key_val,
        upsert_select_options.sort_key_val, Value(spanner_json))};
  } else {
    return Mutations{MakeInsertOrUpdateMutation(
        table_name, upsert_select_options.column_names,
        upsert_select_options.partition_key_val, Value(spanner_json))};
  }
}

void GcpSpannerClientProvider::UpsertDatabaseItemAsync(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
        upsert_database_item_context,
    UpsertSelectOptions upsert_select_options, bool enforce_row_existence,
    nlohmann::json new_attributes) noexcept {
  Client client(*spanner_client_shared_);

  const auto& table_name =
      upsert_database_item_context.request->key().table_name();
  ExecutionResult prepare_result = SuccessExecutionResult();
  auto commit_result_or = client.Commit(
      bind(&GcpSpannerClientProvider::UpsertFunctor, this,
           ref(upsert_database_item_context), ref(client),
           ref(upsert_select_options), enforce_row_existence,
           ref(new_attributes), ref(prepare_result), ref(table_name), _1));

  if (!prepare_result.Successful()) {
    FinishContext(prepare_result, upsert_database_item_context,
                  cpu_async_executor_);
    return;
  }
  if (!commit_result_or.ok()) {
    auto result =
        RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR);
    SCP_ERROR_CONTEXT(
        kGcpSpanner, upsert_database_item_context, result,
        "Spanner upsert commit failed. Error code: %d, message: %s",
        commit_result_or.status().code(),
        commit_result_or.status().message().c_str());
    FinishContext(result, upsert_database_item_context, cpu_async_executor_);
    return;
  }
  FinishContext(SuccessExecutionResult(), upsert_database_item_context,
                cpu_async_executor_);
}

ExecutionResult GcpSpannerClientProvider::UpsertDatabaseItem(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  const auto& request = *upsert_database_item_context.request;
  if (auto execution_result = ValidatePartitionAndSortKey(
          client_options_->table_name_to_keys.get(), request.key());
      !execution_result.Successful()) {
    auto sort_key_str = request.key().has_sort_key()
                            ? request.key().sort_key().name().c_str()
                            : "<empty>";
    SCP_ERROR_CONTEXT(
        kGcpSpanner, upsert_database_item_context, execution_result,
        "Failed validating Partition key (%s) and Sort key (%s) for table %s",
        request.key().partition_key().name().c_str(), sort_key_str,
        request.key().table_name().c_str());
    upsert_database_item_context.result = execution_result;
    upsert_database_item_context.Finish();
    return execution_result;
  }
  // 1.
  //   SELECT Value
  //   FROM `BudgetKeys`
  //   WHERE BudgetKeyId = @partition_key AND
  //   Timeframe = @sort_key # <---- Optional
  //   # If attributes present:
  //   AND JSON_VALUE(Value, '$.token_count') = @attribute_0
  // 2.
  //   Modify Value with elements in new_attributes.
  // 3.
  //   InsertOrUpdate(partition_key, sort_key, Value)

  auto select_options_or =
      UpsertSelectOptions::BuildUpsertSelectOptions(request);
  if (!select_options_or.Successful()) {
    SCP_ERROR_CONTEXT(kGcpSpanner, upsert_database_item_context,
                      select_options_or.result(),
                      "Failed building UpsertSelectOptions");
    upsert_database_item_context.result = select_options_or.result();
    upsert_database_item_context.Finish();
    return select_options_or.result();
  }

  // Row existence should be enforced if attributes is present and non-empty.
  bool enforce_row_existence = !request.required_attributes().empty();

  json new_attributes;
  for (const auto& new_attr : request.new_attributes()) {
    auto json_attr_or =
        GcpSpannerUtils::ConvertItemAttributeToJsonType(new_attr);
    if (!json_attr_or.Successful()) {
      SCP_ERROR_CONTEXT(kGcpSpanner, upsert_database_item_context,
                        json_attr_or.result(),
                        "Error converting ItemAttribute to JSON for table %s",
                        request.key().table_name().c_str());
      upsert_database_item_context.result = json_attr_or.result();
      upsert_database_item_context.Finish();
      return json_attr_or.result();
    }
    new_attributes[new_attr.name()] = move(*json_attr_or);
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpSpannerClientProvider::UpsertDatabaseItemAsync, this,
               upsert_database_item_context, move(*select_options_or),
               enforce_row_existence, move(new_attributes)),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    upsert_database_item_context.result = schedule_result;
    upsert_database_item_context.Finish();
    return schedule_result;
  }

  return SuccessExecutionResult();
}

ExecutionResultOr<pair<shared_ptr<Client>, shared_ptr<DatabaseAdminClient>>>
SpannerFactory::CreateClients(const string& project, const string& instance,
                              const string& database) noexcept {
  return make_pair(
      make_shared<Client>(
          MakeConnection(Database(project, instance, database))),
      make_shared<DatabaseAdminClient>(MakeDatabaseAdminConnection()));
}

shared_ptr<NoSQLDatabaseClientProviderInterface>
NoSQLDatabaseClientProviderFactory::Create(
    const shared_ptr<NoSQLDatabaseClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpSpannerClientProvider>(
      options, instance_client, cpu_async_executor, io_async_executor);
}

}  // namespace google::scp::cpio::client_providers
