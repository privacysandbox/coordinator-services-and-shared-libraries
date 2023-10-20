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

#include "gcp_spanner.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/configuration_keys.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "core/nosql_database_provider/src/common/nosql_database_provider_utils.h"

#include "gcp_spanner_utils.h"

namespace google::scp::core::nosql_database_provider {

namespace {

using google::cloud::spanner::Client;
using google::cloud::spanner::Database;
using SpannerJson = google::cloud::spanner::Json;
using google::cloud::StatusOr;
using google::cloud::spanner::ExponentialBackoffPolicy;
using google::cloud::spanner::LimitedTimeTransactionRerunPolicy;
using google::cloud::spanner::MakeConnection;
using google::cloud::spanner::Mutation;
using google::cloud::spanner::Mutations;
using google::cloud::spanner::RowStream;
using google::cloud::spanner::SqlStatement;
using google::cloud::spanner::Transaction;
using google::cloud::spanner::Value;
using google::scp::core::FinishContext;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common ::TimeProvider;
using google::scp::core::nosql_database_provider::GcpSpannerUtils;
using std::get;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using json = nlohmann::json;
using google::cloud::spanner::MakeInsertOrUpdateMutation;
using std::bind;
using std::make_pair;
using std::make_shared;
using std::move;
using std::optional;
using std::pair;
using std::ref;
using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;
using std::placeholders::_1;

constexpr char kGcpSpanner[] = "GcpSpanner";

constexpr char kValueColumnName[] = "Value";

constexpr char kPartitionKeyParamName[] = "partition_key";
constexpr char kSortKeyParamName[] = "sort_key";

constexpr int kValueColumnIndex = 0;

constexpr std::chrono::milliseconds kTransactionRetryMaxTimeLimitDurationInMs =
    std::chrono::milliseconds(1500);

constexpr std::chrono::milliseconds kTransactionRetryBackOffDurationInMs =
    std::chrono::milliseconds(100);

constexpr double kTransactionRetryBackoffMultiplier = 2.0;

// Returns success if the partition and sort key in req match the stored
// values in table_name_to_keys. Returns success if table_name_to_keys is
// not present.
template <typename Request>
ExecutionResult ValidatePartitionAndSortKey(
    const unordered_map<string, pair<string, optional<string>>>*
        table_name_to_keys,
    const Request& req) {
  if (!table_name_to_keys) return SuccessExecutionResult();

  auto it = table_name_to_keys->find(*req.table_name);
  if (it == table_name_to_keys->end()) {
    return core::FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_TABLE_NOT_FOUND);
  }
  const auto& [partition_key, sort_key_expected] = it->second;
  const auto partition_key_in_request = req.partition_key->attribute_name;
  if (!partition_key_in_request || *partition_key_in_request != partition_key) {
    return core::FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME);
  }

  const auto sort_key_in_request =
      req.sort_key == nullptr ? nullptr : req.sort_key->attribute_name;
  if (!sort_key_expected &&
      (sort_key_in_request != nullptr && !sort_key_in_request->empty())) {
    return core::FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME);
  }
  if (sort_key_expected &&
      (sort_key_in_request == nullptr || sort_key_in_request->empty())) {
    return core::FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME);
  }
  return SuccessExecutionResult();
}

// Given attributes, adds a condition to out to match a member in the Value
// column to the attribute. Also adds these parameters to params.
// All members of attributes are assumed to be nested inside of the Value
// JSON column.
// Add filters of the form:
// "JSON_VALUE(Value, '$.token_count') = @attribute_0"
ExecutionResult AppendJsonWhereClauses(
    shared_ptr<const vector<NoSqlDatabaseKeyValuePair>> attributes,
    SqlStatement::ParamType& params, string& out) {
  if (attributes && attributes->size() > 0) {
    for (size_t attribute_index = 0; attribute_index < attributes->size();
         ++attribute_index) {
      auto attribute_alias = absl::StrCat("attribute_", attribute_index);
      absl::StrAppend(
          &out, absl::StrFormat(" AND JSON_VALUE(Value, '$.%s') = @%s",
                                *attributes->at(attribute_index).attribute_name,
                                attribute_alias));

      Value spanner_val;
      RETURN_IF_FAILURE(
          GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
              *attributes->at(attribute_index).attribute_value, spanner_val));
      params.emplace(attribute_alias, spanner_val);
    }
  }
  return SuccessExecutionResult();
}

}  // namespace

ExecutionResult GcpSpanner::Init() noexcept {
  string project;
  auto execution_result = config_provider_->Get(kGcpProjectId, project);
  if (execution_result != SuccessExecutionResult()) {
    return execution_result;
  }

  string instance;
  execution_result = config_provider_->Get(kSpannerInstance, instance);
  if (execution_result != SuccessExecutionResult()) {
    return execution_result;
  }

  string database;
  execution_result = config_provider_->Get(kSpannerDatabase, database);
  if (execution_result != SuccessExecutionResult()) {
    return execution_result;
  }

  CreateSpanner(project, instance, database);

  return SuccessExecutionResult();
}

void GcpSpanner::CreateSpanner(const std::string& project,
                               const std::string& instance,
                               const std::string& database) noexcept {
  spanner_client_shared_ = make_shared<Client>(
      MakeConnection(Database(project, instance, database)));
}

ExecutionResult GcpSpanner::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpSpanner::Stop() noexcept {
  return SuccessExecutionResult();
}

void GcpSpanner::GetDatabaseItemAsync(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
        get_database_item_context,
    string query, SqlStatement::ParamType params) noexcept {
  Client spanner_client(*spanner_client_shared_);
  auto row_stream =
      spanner_client.ExecuteQuery(SqlStatement(move(query), move(params)));

  auto row_it = row_stream.begin();
  if (row_it == row_stream.end() || !row_it->ok()) {
    ExecutionResult result = FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
    if (!row_it->ok()) {
      result = GcpSpannerUtils::ConvertCloudSpannerErrorToExecutionResult(
          row_it->status().code());
      SCP_ERROR_CONTEXT(
          kGcpSpanner, get_database_item_context, result,
          absl::StrFormat(
              "Spanner get database item request failed. Error code: %d, "
              "message: %s",
              row_it->status().code(), row_it->status().message()));
    }
    FinishContext(result, get_database_item_context, async_executor_,
                  async_execution_priority_);
    return;
  }

  const auto spanner_json_or = row_it->value().get<SpannerJson>(0);
  if (!spanner_json_or.ok()) {
    auto result = GcpSpannerUtils::ConvertCloudSpannerErrorToExecutionResult(
        spanner_json_or.status().code());
    SCP_ERROR_CONTEXT(
        kGcpSpanner, get_database_item_context, result,
        absl::StrFormat("Spanner get JSON Value column failed. Error code: %d, "
                        "message: %s",
                        spanner_json_or.status().code(),
                        spanner_json_or.status().message()));
    FinishContext(result, get_database_item_context, async_executor_,
                  async_execution_priority_);
    return;
  }

  get_database_item_context.response = make_shared<GetDatabaseItemResponse>();
  get_database_item_context.response->table_name =
      get_database_item_context.request->table_name;
  get_database_item_context.response->partition_key =
      get_database_item_context.request->partition_key;
  get_database_item_context.response->sort_key =
      get_database_item_context.request->sort_key;
  get_database_item_context.response->attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();

  json value_json;
  try {
    value_json = json::parse(string(*spanner_json_or));
  } catch (...) {
    FinishContext(
        FailureExecutionResult(errors::SC_NO_SQL_DATABASE_JSON_FAILED_TO_PARSE),
        get_database_item_context, async_executor_, async_execution_priority_);
    return;
  }

  // Populate response attributes from all of the elements in the Value
  // column.
  for (auto& [json_attr_name, json_attr_value] : value_json.items()) {
    auto attr_value = make_shared<NoSQLDatabaseValidAttributeValueTypes>();
    if (!GcpSpannerUtils::ConvertJsonTypeToNoSQLDatabaseValidAttributeValueType(
             json_attr_value, *attr_value)
             .Successful()) {
      // If conversion fails, it is likely a list, struct, or other
      // unsupported type. Continue without failing.
      // TODO Log this?
      continue;
    }
    NoSqlDatabaseKeyValuePair& key_value_pair =
        get_database_item_context.response->attributes->emplace_back();
    key_value_pair.attribute_name = make_shared<string>(json_attr_name);
    key_value_pair.attribute_value = attr_value;
  }

  // Executed on non-IO pool to keep it separate from IO aspects.
  FinishContext(SuccessExecutionResult(), get_database_item_context,
                async_executor_, async_execution_priority_);
}

ExecutionResult GcpSpanner::GetDatabaseItem(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  if (auto execution_result = ValidatePartitionAndSortKey(
          table_name_to_keys_.get(), *get_database_item_context.request);
      !execution_result.Successful()) {
    return execution_result;
  }
  // Build a query like:
  // SELECT IFNULL(Value, JSON '{}')
  // FROM BudgetKeys
  // WHERE BudgetKeyId = @partition_key
  //   AND Timeframe = @sort_key
  //   AND JSON_VALUE(Value, '$.token_count') = @attribute_0
  //   AND JSON_VALUE(Value, '$.grammy_awards') = @attribute_1

  // Set the table name
  const auto& table_name = *get_database_item_context.request->table_name;
  string query = absl::StrFormat("SELECT IFNULL(%s, JSON '{}') FROM %s WHERE ",
                                 kValueColumnName, table_name);

  // Set the partition key
  auto& provided_partition_key =
      get_database_item_context.request->partition_key;
  Value spanner_part_key_val;
  if (auto execution_result =
          GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
              *provided_partition_key->attribute_value, spanner_part_key_val);
      !execution_result.Successful()) {
    return execution_result;
  }
  query += absl::StrCat(*provided_partition_key->attribute_name, " = @",
                        kPartitionKeyParamName);

  SqlStatement::ParamType params;
  params.emplace(kPartitionKeyParamName, spanner_part_key_val);

  // sort_key is optional
  auto& provided_sort_key = get_database_item_context.request->sort_key;
  if (provided_sort_key != nullptr) {
    Value spanner_sort_key_val;
    if (auto execution_result = GcpSpannerUtils::
            ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
                *provided_sort_key->attribute_value, spanner_sort_key_val);
        !execution_result.Successful()) {
      return execution_result;
    }

    absl::StrAppend(&query, " AND ", *provided_sort_key->attribute_name, " = @",
                    kSortKeyParamName);
    params.emplace(kSortKeyParamName, spanner_sort_key_val);
  }

  // Set the filter expression
  if (auto execution_result = AppendJsonWhereClauses(
          get_database_item_context.request->attributes, params, query);
      !execution_result.Successful()) {
    return execution_result;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpSpanner::GetDatabaseItemAsync, this,
               get_database_item_context, move(query), move(params)),
          io_async_execution_priority_);
      !schedule_result.Successful()) {
    get_database_item_context.result = schedule_result;
    get_database_item_context.Finish();
    return schedule_result;
  }

  return SuccessExecutionResult();
}

ExecutionResultOr<GcpSpanner::UpsertSelectOptions>
GcpSpanner::UpsertSelectOptions::BuildUpsertSelectOptions(
    const UpsertDatabaseItemRequest& request) {
  UpsertSelectOptions upsert_select_options;
  const auto& table_name = *request.table_name;

  string select_query =
      absl::StrFormat("SELECT %s FROM %s WHERE ", kValueColumnName, table_name);
  SqlStatement::ParamType params;

  // Set the partition key
  auto& provided_partition_key = request.partition_key;
  RETURN_IF_FAILURE(
      GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
          *provided_partition_key->attribute_value,
          upsert_select_options.partition_key_val));
  absl::StrAppendFormat(&select_query, "%s = @%s",
                        *provided_partition_key->attribute_name,
                        kPartitionKeyParamName);
  params.emplace(kPartitionKeyParamName,
                 upsert_select_options.partition_key_val);
  upsert_select_options.column_names.push_back(
      *provided_partition_key->attribute_name);

  // sort_key is optional
  auto& provided_sort_key = request.sort_key;
  if (provided_sort_key != nullptr &&
      !provided_sort_key->attribute_name->empty()) {
    RETURN_IF_FAILURE(
        GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
            *provided_sort_key->attribute_value,
            upsert_select_options.sort_key_val));
    absl::StrAppendFormat(&select_query, " AND %s = @%s",
                          *provided_sort_key->attribute_name,
                          kSortKeyParamName);
    params.emplace(kSortKeyParamName, upsert_select_options.sort_key_val);
    upsert_select_options.column_names.push_back(
        *provided_sort_key->attribute_name);
  }
  upsert_select_options.column_names.push_back(kValueColumnName);

  // Set the filter expression
  RETURN_IF_FAILURE(
      AppendJsonWhereClauses(request.attributes, params, select_query));

  upsert_select_options.select_statement =
      SqlStatement(move(select_query), move(params));

  return upsert_select_options;
}

ExecutionResult GcpSpanner::GetMergedJson(RowStream& row_stream,
                                          bool enforce_row_existence,
                                          json new_attributes,
                                          optional<SpannerJson>& spanner_json) {
  auto row_it = row_stream.begin();
  if (!row_it->ok()) {
    auto result = GcpSpannerUtils::ConvertCloudSpannerErrorToExecutionResult(
        row_it->status().code());
    SCP_ERROR(
        kGcpSpanner, kZeroUuid, result,
        absl::StrFormat(
            "Spanner upsert database item request failed. Error code: %d, "
            "message: %s",
            row_it->status().code(), row_it->status().message()));
    return result;
  }
  if (row_it == row_stream.end()) {
    if (enforce_row_existence) {
      return FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
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
    auto result = FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_CORRUPTED);
    SCP_ERROR(
        kGcpSpanner, kZeroUuid, result,
        absl::StrFormat("Spanner get JSON Value column failed. Error code: %d, "
                        "message: %s",
                        spanner_json_or.status().code(),
                        spanner_json_or.status().message()));
    return result;
  }

  try {
    existing_json = json::parse(string(*spanner_json_or));
  } catch (...) {
    return FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_JSON_FAILED_TO_PARSE);
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

Mutations GcpSpanner::UpsertFunctor(
    Client& client, const UpsertSelectOptions& upsert_select_options,
    bool enforce_row_existence, const nlohmann::json& new_attributes,
    ExecutionResult& prepare_result, const string& table_name,
    Transaction txn) {
  auto row_stream =
      client.ExecuteQuery(txn, upsert_select_options.select_statement);

  optional<SpannerJson> spanner_json;
  prepare_result = GetMergedJson(row_stream, enforce_row_existence,
                                 move(new_attributes), spanner_json);
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

void GcpSpanner::UpsertDatabaseItemAsync(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
        upsert_database_item_context,
    UpsertSelectOptions upsert_select_options, bool enforce_row_existence,
    nlohmann::json new_attributes) noexcept {
  Client client(*spanner_client_shared_);

  const auto& table_name = *upsert_database_item_context.request->table_name;
  ExecutionResult prepare_result = SuccessExecutionResult();
  auto commit_result_or = client.Commit(
      bind(&GcpSpanner::UpsertFunctor, ref(client), ref(upsert_select_options),
           enforce_row_existence, ref(new_attributes), ref(prepare_result),
           ref(table_name), _1),
      LimitedTimeTransactionRerunPolicy(
          kTransactionRetryMaxTimeLimitDurationInMs)
          .clone(),
      ExponentialBackoffPolicy(kTransactionRetryBackOffDurationInMs,
                               kTransactionRetryMaxTimeLimitDurationInMs,
                               kTransactionRetryBackoffMultiplier)
          .clone());

  if (!prepare_result.Successful()) {
    FinishContext(prepare_result, upsert_database_item_context, async_executor_,
                  async_execution_priority_);
    return;
  }
  if (!commit_result_or.ok()) {
    auto result =
        core::RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR);
    SCP_ERROR_CONTEXT(
        kGcpSpanner, upsert_database_item_context, result,
        absl::StrFormat("Spanner upsert commit failed. Error code: %d, "
                        "message: %s",
                        commit_result_or.status().code(),
                        commit_result_or.status().message()));
    FinishContext(result, upsert_database_item_context, async_executor_,
                  async_execution_priority_);
    return;
  }
  FinishContext(SuccessExecutionResult(), upsert_database_item_context,
                async_executor_, async_execution_priority_);
}

ExecutionResult GcpSpanner::UpsertDatabaseItem(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  RETURN_IF_FAILURE(ValidatePartitionAndSortKey(
      table_name_to_keys_.get(), *upsert_database_item_context.request));
  // 1.
  //   SELECT Value
  //   FROM BudgetKeys
  //   WHERE BudgetKeyId = @partition_key AND
  //   Timeframe = @sort_key # <---- Optional
  //   # If attributes present:
  //   AND JSON_VALUE(Value, '$.token_count') = @attribute_0
  // 2.
  //   Modify Value with elements in new_attributes.
  // 3.
  //   InsertOrUpdate(partition_key, sort_key, Value)

  auto select_options_or = UpsertSelectOptions::BuildUpsertSelectOptions(
      *upsert_database_item_context.request);
  RETURN_IF_FAILURE(select_options_or.result());

  // Row existence should be enforced if attributes is present and non-empty.
  bool enforce_row_existence =
      (upsert_database_item_context.request->attributes != nullptr &&
       !upsert_database_item_context.request->attributes->empty());

  json new_attributes;
  if (upsert_database_item_context.request->new_attributes) {
    for (const auto& new_attr :
         *upsert_database_item_context.request->new_attributes) {
      RETURN_IF_FAILURE(
          GcpSpannerUtils::
              ConvertNoSQLDatabaseValidAttributeValueTypeToJsonType(
                  *new_attr.attribute_value,
                  new_attributes[*new_attr.attribute_name]));
    }
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpSpanner::UpsertDatabaseItemAsync, this,
               upsert_database_item_context, move(*select_options_or),
               enforce_row_existence, move(new_attributes)),
          io_async_execution_priority_);
      !schedule_result.Successful()) {
    upsert_database_item_context.result = schedule_result;
    upsert_database_item_context.Finish();
    return schedule_result;
  }

  return SuccessExecutionResult();
}

}  // namespace google::scp::core::nosql_database_provider
