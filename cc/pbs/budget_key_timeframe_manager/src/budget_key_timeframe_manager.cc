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

#include "budget_key_timeframe_manager.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <functional>
#include <list>
#include <memory>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "pbs/budget_key_timeframe_manager/src/proto/budget_key_timeframe_manager.pb.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/metrics_def.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/src/metric_utils.h"
#include "public/cpio/utils/metric_aggregation/src/simple_metric.h"

#include "budget_key_timeframe_serialization.h"
#include "budget_key_timeframe_utils.h"
#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::CheckpointLog;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetDatabaseItemRequest;
using google::scp::core::GetDatabaseItemResponse;
using google::scp::core::JournalLogRequest;
using google::scp::core::JournalLogResponse;
using google::scp::core::JournalLogStatus;
using google::scp::core::NoSqlDatabaseKeyValuePair;
using google::scp::core::NoSQLDatabaseValidAttributeValueTypes;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::UpsertDatabaseItemRequest;
using google::scp::core::UpsertDatabaseItemResponse;
using google::scp::core::Version;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::pbs::budget_key_timeframe_manager::Serialization;
using google::scp::pbs::budget_key_timeframe_manager::Utils;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeGroupLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeManagerLog;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeManagerLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::OperationType;
using std::function;
using std::get;
using std::list;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::unordered_set;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;

static constexpr char kBudgetKeyPartitionKey[] = "Budget_Key";
static constexpr char kTimeframeSortKey[] = "Timeframe";
static constexpr char kToken[] = "TokenCount";
static constexpr char kBudgetKeyTimeframeManager[] =
    "BudgetKeyTimeframeManager";

namespace google::scp::pbs {
ExecutionResult BudgetKeyTimeframeManager::Init() noexcept {
  auto execution_result = budget_key_timeframe_groups_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  return journal_service_->SubscribeForRecovery(
      id_, bind(&BudgetKeyTimeframeManager::OnJournalServiceRecoverCallback,
                this, _1, _2));
}

ExecutionResult BudgetKeyTimeframeManager::Run() noexcept {
  return budget_key_timeframe_groups_->Run();
}

ExecutionResult BudgetKeyTimeframeManager::Stop() noexcept {
  return budget_key_timeframe_groups_->Stop();
}

BudgetKeyTimeframeManager::~BudgetKeyTimeframeManager() {
  if (journal_service_) {
    // Ignore the failure.
    journal_service_->UnsubscribeForRecovery(id_);
  }
}

void BudgetKeyTimeframeManager::OnBeforeGarbageCollection(
    TimeGroup& time_group,
    shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
    function<void(bool)> should_delete_entry) noexcept {
  SCP_DEBUG(
      kBudgetKeyTimeframeManager, id_,
      "Unloading budget key timeframe for budget key %s with time_group %llu",
      budget_key_name_->c_str(), time_group);

  // Check to see if there is any active transaction id.
  vector<TimeBucket> time_buckets;
  auto execution_result =
      budget_key_timeframe_group->budget_key_timeframes.Keys(time_buckets);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }

  for (auto time_bucket : time_buckets) {
    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    execution_result = budget_key_timeframe_group->budget_key_timeframes.Find(
        time_bucket, budget_key_timeframe);
    if (!execution_result.Successful()) {
      should_delete_entry(false);
      return;
    }
    if (budget_key_timeframe->active_transaction_id.load() != kZeroUuid) {
      should_delete_entry(false);
      return;
    }
  }

  time_buckets.clear();
  execution_result =
      budget_key_timeframe_group->budget_key_timeframes.Keys(time_buckets);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }

  std::sort(time_buckets.begin(), time_buckets.end());
  vector<TokenCount> token_counts;
  token_counts.reserve(time_buckets.size());
  for (auto time_bucket : time_buckets) {
    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    execution_result = budget_key_timeframe_group->budget_key_timeframes.Find(
        time_bucket, budget_key_timeframe);
    if (!execution_result.Successful()) {
      should_delete_entry(false);
      return;
    }

    token_counts.push_back(budget_key_timeframe->token_count);
  }

  string serialized_tokens;
  execution_result = Serialization::SerializeHourTokensInTimeGroup(
      token_counts, serialized_tokens);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }

  auto key_table_name = make_shared<string>();
  execution_result =
      config_provider_->Get(kBudgetKeyTableName, *key_table_name);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(
          make_shared<UpsertDatabaseItemRequest>(),
          bind(&BudgetKeyTimeframeManager::OnStoreTimeframeGroupToDBCallback,
               this, _1, time_group, budget_key_timeframe_group,
               should_delete_entry),
          id_, id_);

  upsert_database_item_context.request->table_name = key_table_name;
  upsert_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>();
  upsert_database_item_context.request->partition_key->attribute_name =
      make_shared<string>(kBudgetKeyPartitionKey);
  upsert_database_item_context.request->partition_key->attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(*budget_key_name_);
  upsert_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>();
  upsert_database_item_context.request->sort_key->attribute_name =
      make_shared<string>(kTimeframeSortKey);
  auto time_group_str = std::to_string(time_group);
  upsert_database_item_context.request->sort_key->attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(time_group_str);
  upsert_database_item_context.request->new_attributes =
      make_shared<vector<NoSqlDatabaseKeyValuePair>>();
  upsert_database_item_context.callback =
      bind(&BudgetKeyTimeframeManager::OnStoreTimeframeGroupToDBCallback, this,
           _1, time_group, budget_key_timeframe_group, should_delete_entry);

  NoSqlDatabaseKeyValuePair key_value_pair;
  key_value_pair.attribute_name = make_shared<string>(kToken);
  key_value_pair.attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(serialized_tokens);
  upsert_database_item_context.request->new_attributes->push_back(
      key_value_pair);
  budget_key_count_metric_->Increment(kMetricEventUnloadFromDBScheduled);

  // Request-level retry is not necessary here. If the request is unsuccessful,
  // retry in next round of OnBeforeGarbageCollection.
  execution_result =
      nosql_database_provider_for_background_operations_->UpsertDatabaseItem(
          upsert_database_item_context);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }
}

void BudgetKeyTimeframeManager::OnStoreTimeframeGroupToDBCallback(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context,
    TimeGroup& time_group,
    shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
    function<void(bool)> should_delete_entry) noexcept {
  if (upsert_database_item_context.result != SuccessExecutionResult()) {
    budget_key_count_metric_->Increment(kMetricEventUnloadFromDBFailed);
    should_delete_entry(false);
    return;
  }

  budget_key_count_metric_->Increment(kMetricEventUnloadFromDBSuccess);

  BytesBuffer budget_key_timeframe_manager_log_bytes_buffer;
  auto execution_result =
      Serialization::SerializeBudgetKeyTimeframeGroupRemoval(
          budget_key_timeframe_group,
          budget_key_timeframe_manager_log_bytes_buffer);
  if (execution_result != SuccessExecutionResult()) {
    SCP_ERROR_CONTEXT(kBudgetKeyTimeframeManager, upsert_database_item_context,
                      execution_result,
                      "Failed to serialize budget key removal");
    should_delete_entry(false);
    return;
  }

  // Sending the journal service log.
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.parent_activity_id =
      upsert_database_item_context.activity_id;
  journal_log_context.correlation_id =
      upsert_database_item_context.correlation_id;
  journal_log_context.request->component_id = id_;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data = make_shared<BytesBuffer>();
  journal_log_context.request->data->bytes =
      budget_key_timeframe_manager_log_bytes_buffer.bytes;
  journal_log_context.request->data->length =
      budget_key_timeframe_manager_log_bytes_buffer.length;
  journal_log_context.request->data->capacity =
      budget_key_timeframe_manager_log_bytes_buffer.capacity;
  journal_log_context.callback =
      bind(&BudgetKeyTimeframeManager::OnRemoveEntryFromCacheLogged, this,
           should_delete_entry, _1);

  // Request-level retry is not necessary here. If the request is unsuccessful,
  // retry in next round of OnBeforeGarbageCollection.
  execution_result = journal_service_->Log(journal_log_context);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }
}

void BudgetKeyTimeframeManager::OnRemoveEntryFromCacheLogged(
    std::function<void(bool)> should_delete_entry,
    AsyncContext<JournalLogRequest, JournalLogResponse>&
        journal_log_context) noexcept {
  should_delete_entry(journal_log_context.result.Successful());
}

ExecutionResult BudgetKeyTimeframeManager::CanUnload() noexcept {
  vector<TimeBucket> keys;
  auto execution_result = budget_key_timeframe_groups_->Keys(keys);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (keys.size() > 0) {
    return FailureExecutionResult(
        core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CANNOT_BE_UNLOADED);
  }

  return SuccessExecutionResult();
}

ExecutionResult
BudgetKeyTimeframeManager::PopulateLoadBudgetKeyTimeframeResponse(
    const shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
    const shared_ptr<LoadBudgetKeyTimeframeRequest>&
        load_budget_key_timeframe_request,
    shared_ptr<LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_response) {
  vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes;
  for (const auto& reporting_time :
       load_budget_key_timeframe_request->reporting_times) {
    auto time_bucket = Utils::GetTimeBucket(reporting_time);
    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    auto execution_result =
        budget_key_timeframe_group->budget_key_timeframes.Find(
            time_bucket, budget_key_timeframe);
    if (execution_result != SuccessExecutionResult()) {
      return execution_result;
    }
    budget_key_timeframes.push_back(move(budget_key_timeframe));
  }

  // Set the response
  LoadBudgetKeyTimeframeResponse load_budget_key_frames_response;
  load_budget_key_frames_response.budget_key_frames = budget_key_timeframes;

  load_budget_key_timeframe_response =
      make_shared<LoadBudgetKeyTimeframeResponse>(
          move(load_budget_key_frames_response));

  return SuccessExecutionResult();
}

ExecutionResult BudgetKeyTimeframeManager::Load(
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  // To prevent many threads to load any time frame from the database, it is
  // required to ensure only one thread goes to the database and every other
  // thread will wait on the result. In this case a concurrent map is used. If
  // any thread successfully inserts an entry into the cache it will be the
  // only one which goes to the database. The rest of the threads will retry
  // until the entry is loaded.

  if (load_budget_key_timeframe_context.request->reporting_times.empty()) {
    return FailureExecutionResult(
        core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_EMPTY_REQUEST);
  }

  if (load_budget_key_timeframe_context.request->reporting_times.size() > 1) {
    // Can support loading only one time group in a request.
    auto unique_time_groups = Utils::GetUniqueTimeGroups(
        load_budget_key_timeframe_context.request->reporting_times);
    if (unique_time_groups.size() != 1) {
      return FailureExecutionResult(
          core::errors::
              SC_BUDGET_KEY_TIMEFRAME_MANAGER_MULTIPLE_TIMEFRAME_GROUPS);
    }

    // Loads must be on unique time buckets
    auto unique_time_buckets = Utils::GetUniqueTimeBuckets(
        load_budget_key_timeframe_context.request->reporting_times);
    if (unique_time_buckets.size() !=
        load_budget_key_timeframe_context.request->reporting_times.size()) {
      return FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_REPEATED_TIMEBUCKETS);
    }
  }

  TimeGroup time_group = Utils::GetTimeGroup(
      load_budget_key_timeframe_context.request->reporting_times.front());
  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, budget_key_timeframe_group);

  // Regardless of outcome we will insert into the map. The outcome can be
  // success or failure but in both cases the element will be in the map.
  auto execution_result = budget_key_timeframe_groups_->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  if (!execution_result.Successful()) {
    if (execution_result.status_code !=
            core::errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED &&
        execution_result.status_code !=
            core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS) {
      return execution_result;
    }

    if (execution_result.status_code ==
        core::errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED) {
      return RetryExecutionResult(execution_result.status_code);
    }

    auto should_load = false;
    if (budget_key_timeframe_group->needs_loader) {
      auto needs_loader = true;
      if (budget_key_timeframe_group->needs_loader.compare_exchange_strong(
              needs_loader, false)) {
        should_load = true;
      }
    }

    if (!should_load) {
      if (!budget_key_timeframe_group->is_loaded) {
        return RetryExecutionResult(
            core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_ENTRY_IS_LOADING);
      }

      execution_result = PopulateLoadBudgetKeyTimeframeResponse(
          budget_key_timeframe_group, load_budget_key_timeframe_context.request,
          load_budget_key_timeframe_context.response);
      if (!execution_result.Successful()) {
        return execution_result;
      }

      load_budget_key_timeframe_context.result = SuccessExecutionResult();
      load_budget_key_timeframe_context.Finish();
      return SuccessExecutionResult();
    }
  }

  execution_result = budget_key_timeframe_groups_->DisableEviction(
      budget_key_timeframe_group_pair.first);
  if (!execution_result.Successful()) {
    return RetryExecutionResult(execution_result.status_code);
  }

  return LoadTimeframeGroupFromDB(load_budget_key_timeframe_context,
                                  budget_key_timeframe_group);
}

ExecutionResult BudgetKeyTimeframeManager::LoadTimeframeGroupFromDB(
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context,
    shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group) noexcept {
  auto time_frame_manager_id_str = core::common::ToString(id_);
  SCP_DEBUG_CONTEXT(
      kBudgetKeyTimeframeManager, load_budget_key_timeframe_context,
      "Timeframe manager %s loading budget key name %s with time_group %llu",
      time_frame_manager_id_str.c_str(), budget_key_name_->c_str(),
      budget_key_timeframe_group->time_group);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          make_shared<GetDatabaseItemRequest>(),
          bind(&BudgetKeyTimeframeManager::OnLoadTimeframeGroupFromDBCallback,
               this, load_budget_key_timeframe_context,
               budget_key_timeframe_group, _1),
          load_budget_key_timeframe_context);

  auto key_table_name = make_shared<string>();
  auto execution_result =
      config_provider_->Get(kBudgetKeyTableName, *key_table_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  get_database_item_context.request->table_name = key_table_name;
  get_database_item_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>();
  get_database_item_context.request->partition_key->attribute_name =
      make_shared<string>(kBudgetKeyPartitionKey);
  get_database_item_context.request->partition_key->attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(*budget_key_name_);

  get_database_item_context.request->sort_key =
      make_shared<NoSqlDatabaseKeyValuePair>();
  get_database_item_context.request->sort_key->attribute_name =
      make_shared<string>(kTimeframeSortKey);
  auto time_group = std::to_string(budget_key_timeframe_group->time_group);
  get_database_item_context.request->sort_key->attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(time_group);

  budget_key_count_metric_->Increment(kMetricEventLoadFromDBScheduled);

  return nosql_database_provider_for_live_traffic_->GetDatabaseItem(
      get_database_item_context);
}

void BudgetKeyTimeframeManager::OnLoadTimeframeGroupFromDBCallback(
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context,
    std::shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  auto execution_result = budget_key_timeframe_groups_->EnableEviction(
      budget_key_timeframe_group->time_group);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kBudgetKeyTimeframeManager, get_database_item_context, execution_result,
        "Cache eviction failed for %s time_group %d", budget_key_name_->c_str(),
        budget_key_timeframe_group->time_group);
  }

  if (!get_database_item_context.result.Successful()) {
    if (get_database_item_context.result.status_code !=
        core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND) {
      budget_key_count_metric_->Increment(kMetricEventLoadFromDBFailed);
      budget_key_timeframe_group->needs_loader = true;
      load_budget_key_timeframe_context.result =
          get_database_item_context.result;
      load_budget_key_timeframe_context.Finish();
      return;
    }
  }

  budget_key_count_metric_->Increment(kMetricEventLoadFromDBSuccess);

  vector<TokenCount> tokens_per_hour;
  if (get_database_item_context.result.status_code ==
      core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND) {
    tokens_per_hour = vector<TokenCount>(
        budget_key_timeframe_manager::kHoursPerDay, kMaxToken);
  } else {
    string token_value;
    for (auto attribute : *get_database_item_context.response->attributes) {
      if (attribute.attribute_name->compare(kToken) == 0) {
        token_value = get<string>(*attribute.attribute_value);
        break;
      }
    }

    if (token_value.size() == 0) {
      budget_key_timeframe_group->needs_loader = true;
      load_budget_key_timeframe_context.result = FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
      load_budget_key_timeframe_context.Finish();
      return;
    }

    tokens_per_hour.clear();
    execution_result = Serialization::DeserializeHourTokensInTimeGroup(
        token_value, tokens_per_hour);
    if (!execution_result.Successful()) {
      budget_key_timeframe_group->needs_loader = true;
      load_budget_key_timeframe_context.result = execution_result;
      load_budget_key_timeframe_context.Finish();
      return;
    }
  }

  for (TimeBucket i = 0; i < budget_key_timeframe_manager::kHoursPerDay; ++i) {
    auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(i);
    auto budget_key_timeframe_pair = make_pair(i, budget_key_timeframe);
    execution_result =
        budget_key_timeframe_group->budget_key_timeframes.Erase(i);
    if (!execution_result.Successful()) {
      if (execution_result !=
          FailureExecutionResult(
              core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)) {
        budget_key_timeframe_group->needs_loader = true;
        load_budget_key_timeframe_context.result = execution_result;
        load_budget_key_timeframe_context.Finish();
        return;
      }
    }

    execution_result = budget_key_timeframe_group->budget_key_timeframes.Insert(
        budget_key_timeframe_pair, budget_key_timeframe);
    if (!execution_result.Successful()) {
      budget_key_timeframe_group->needs_loader = true;
      load_budget_key_timeframe_context.result = execution_result;
      load_budget_key_timeframe_context.Finish();
      return;
    }

    budget_key_timeframe->active_token_count = 0;
    budget_key_timeframe->active_transaction_id = kZeroUuid;
    budget_key_timeframe->token_count = tokens_per_hour[i];
  }

  execution_result = PopulateLoadBudgetKeyTimeframeResponse(
      budget_key_timeframe_group, load_budget_key_timeframe_context.request,
      load_budget_key_timeframe_context.response);
  if (execution_result != SuccessExecutionResult()) {
    budget_key_timeframe_group->needs_loader = true;
    load_budget_key_timeframe_context.result = execution_result;
    load_budget_key_timeframe_context.Finish();
    return;
  }

  // Journal must be written
  BytesBuffer journal_log_bytes_buffer;
  execution_result = Serialization::SerializeBudgetKeyTimeframeGroupLog(
      budget_key_timeframe_group, journal_log_bytes_buffer);
  if (!execution_result.Successful()) {
    budget_key_timeframe_group->needs_loader = true;
    load_budget_key_timeframe_context.result = execution_result;
    load_budget_key_timeframe_context.Finish();
    return;
  }

  // Sending the journal service log.
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.parent_activity_id =
      load_budget_key_timeframe_context.activity_id;
  journal_log_context.correlation_id =
      load_budget_key_timeframe_context.correlation_id;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.request->component_id = id_;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data = make_shared<BytesBuffer>();
  journal_log_context.request->data->bytes = journal_log_bytes_buffer.bytes;
  journal_log_context.request->data->length = journal_log_bytes_buffer.length;
  journal_log_context.request->data->capacity =
      journal_log_bytes_buffer.capacity;
  journal_log_context.callback =
      bind(&BudgetKeyTimeframeManager::OnLogLoadCallback, this,
           load_budget_key_timeframe_context, budget_key_timeframe_group, _1);

  operation_dispatcher_
      .Dispatch<AsyncContext<JournalLogRequest, JournalLogResponse>>(
          journal_log_context,
          [journal_service = journal_service_](
              AsyncContext<JournalLogRequest, JournalLogResponse>&
                  journal_log_context) {
            return journal_service->Log(journal_log_context);
          });
}

void BudgetKeyTimeframeManager::OnLogLoadCallback(
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context,
    shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
    AsyncContext<JournalLogRequest, JournalLogResponse>&
        journal_log_context) noexcept {
  if (!journal_log_context.result.Successful()) {
    budget_key_timeframe_group->needs_loader = true;
    load_budget_key_timeframe_context.result = journal_log_context.result;
    load_budget_key_timeframe_context.Finish();
    return;
  }

  budget_key_timeframe_group->needs_loader = false;
  budget_key_timeframe_group->is_loaded = true;
  load_budget_key_timeframe_context.result = SuccessExecutionResult();
  load_budget_key_timeframe_context.Finish();
}

ExecutionResult BudgetKeyTimeframeManager::Update(
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context) noexcept {
  if (update_budget_key_timeframe_context.request->timeframes_to_update
          .empty()) {
    return FailureExecutionResult(
        core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_EMPTY_REQUEST);
  }

  bool is_batch_request =
      update_budget_key_timeframe_context.request->timeframes_to_update.size() >
      1;

  if (is_batch_request) {
    std::vector<TimeBucket> reporting_times;
    for (const auto& timeframe :
         update_budget_key_timeframe_context.request->timeframes_to_update) {
      reporting_times.push_back(timeframe.reporting_time);
    }

    // Can support loading only one time group in a request.
    auto unique_time_groups = Utils::GetUniqueTimeGroups(reporting_times);
    if (unique_time_groups.size() != 1) {
      return FailureExecutionResult(
          core::errors::
              SC_BUDGET_KEY_TIMEFRAME_MANAGER_MULTIPLE_TIMEFRAME_GROUPS);
    }

    // Updates must be on unique time buckets
    auto unique_time_buckets = Utils::GetUniqueTimeBuckets(reporting_times);
    if (unique_time_buckets.size() !=
        update_budget_key_timeframe_context.request->timeframes_to_update
            .size()) {
      return FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_REPEATED_TIMEBUCKETS);
    }
  }

  TimeGroup time_group = Utils::GetTimeGroup(
      update_budget_key_timeframe_context.request->timeframes_to_update.front()
          .reporting_time);
  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
  auto execution_result = budget_key_timeframe_groups_->Find(
      time_group, budget_key_timeframe_group);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  vector<shared_ptr<BudgetKeyTimeframe>> original_budget_key_timeframes;
  vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes_to_journal;
  for (const auto& timeframe_to_update :
       update_budget_key_timeframe_context.request->timeframes_to_update) {
    auto time_bucket = Utils::GetTimeBucket(timeframe_to_update.reporting_time);

    shared_ptr<BudgetKeyTimeframe> original_budget_key_timeframe;
    execution_result = budget_key_timeframe_group->budget_key_timeframes.Find(
        time_bucket, original_budget_key_timeframe);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    auto modified_budget_key_timeframe =
        make_shared<BudgetKeyTimeframe>(time_bucket);
    modified_budget_key_timeframe->active_token_count =
        timeframe_to_update.active_token_count;
    modified_budget_key_timeframe->active_transaction_id =
        timeframe_to_update.active_transaction_id;
    modified_budget_key_timeframe->token_count =
        timeframe_to_update.token_count;

    original_budget_key_timeframes.push_back(original_budget_key_timeframe);
    budget_key_timeframes_to_journal.push_back(modified_budget_key_timeframe);
  }

  execution_result = budget_key_timeframe_groups_->DisableEviction(time_group);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  BytesBuffer budget_key_timeframe_manager_log_bytes_buffer;
  if (!is_batch_request) {
    execution_result = Serialization::SerializeBudgetKeyTimeframeLog(
        time_group, budget_key_timeframes_to_journal[0],
        budget_key_timeframe_manager_log_bytes_buffer);
    if (!execution_result.Successful()) {
      return execution_result;
    }
  } else {
    execution_result = Serialization::SerializeBatchBudgetKeyTimeframeLog(
        time_group, budget_key_timeframes_to_journal,
        budget_key_timeframe_manager_log_bytes_buffer);
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  // Sending the journal service log.
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.parent_activity_id =
      update_budget_key_timeframe_context.activity_id;
  journal_log_context.correlation_id =
      update_budget_key_timeframe_context.correlation_id;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.request->component_id = id_;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data = make_shared<BytesBuffer>();
  journal_log_context.request->data->bytes =
      budget_key_timeframe_manager_log_bytes_buffer.bytes;
  journal_log_context.request->data->length =
      budget_key_timeframe_manager_log_bytes_buffer.length;
  journal_log_context.request->data->capacity =
      budget_key_timeframe_manager_log_bytes_buffer.capacity;
  journal_log_context.callback = bind(
      &BudgetKeyTimeframeManager::OnLogUpdateCallback, this,
      update_budget_key_timeframe_context, original_budget_key_timeframes, _1);

  operation_dispatcher_
      .Dispatch<AsyncContext<JournalLogRequest, JournalLogResponse>>(
          journal_log_context,
          [journal_service = journal_service_](
              AsyncContext<JournalLogRequest, JournalLogResponse>&
                  journal_log_context) {
            return journal_service->Log(journal_log_context);
          });

  return SuccessExecutionResult();
}

void BudgetKeyTimeframeManager::OnLogUpdateCallback(
    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>&
        update_budget_key_timeframe_context,
    vector<shared_ptr<BudgetKeyTimeframe>>& budget_key_timeframes,
    AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
        journal_log_context) noexcept {
  // All of the time frames must be from the same time group so picking the
  // first one.
  auto time_group = Utils::GetTimeGroup(
      update_budget_key_timeframe_context.request->timeframes_to_update.front()
          .reporting_time);
  auto execution_result =
      budget_key_timeframe_groups_->EnableEviction(time_group);

  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kBudgetKeyTimeframeManager,
                      update_budget_key_timeframe_context, execution_result,
                      "Cache eviction failed for %s time group %d",
                      budget_key_name_->c_str(), time_group);
  }

  if (!journal_log_context.result.Successful()) {
    update_budget_key_timeframe_context.result = journal_log_context.result;
    update_budget_key_timeframe_context.Finish();
    return;
  }

  // Now that the journal has been written, update the timeframes in memory
  for (unsigned int i = 0; i < budget_key_timeframes.size(); i++) {
    budget_key_timeframes[i]->active_token_count =
        update_budget_key_timeframe_context.request->timeframes_to_update[i]
            .active_token_count;
    budget_key_timeframes[i]->token_count =
        update_budget_key_timeframe_context.request->timeframes_to_update[i]
            .token_count;
    // The following release lock on this time frame if the value is set
    // to 0, perform all of the other modifications before this.
    budget_key_timeframes[i]->active_transaction_id =
        update_budget_key_timeframe_context.request->timeframes_to_update[i]
            .active_transaction_id;
  }

  update_budget_key_timeframe_context.result = SuccessExecutionResult();
  update_budget_key_timeframe_context.Finish();
}

ExecutionResult BudgetKeyTimeframeManager::OnJournalServiceRecoverCallback(
    const shared_ptr<BytesBuffer>& bytes_buffer,
    const Uuid& activity_id) noexcept {
  SCP_DEBUG(kBudgetKeyTimeframeManager, activity_id,
            "Recovering budget key timeframe manager from the stored logs. The "
            "current bytes size: %zu.",
            bytes_buffer->length);

  BudgetKeyTimeframeManagerLog budget_key_time_frame_manager_log;
  auto execution_result =
      Serialization::DeserializeBudgetKeyTimeframeManagerLog(
          *bytes_buffer, budget_key_time_frame_manager_log);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  BudgetKeyTimeframeManagerLog_1_0 budget_key_time_frame_manager_log_1_0;
  execution_result = Serialization::DeserializeBudgetKeyTimeframeManagerLog_1_0(
      budget_key_time_frame_manager_log.log_body(),
      budget_key_time_frame_manager_log_1_0);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (budget_key_time_frame_manager_log_1_0.operation_type() ==
      OperationType::INSERT_TIMEGROUP_INTO_CACHE) {
    TimeGroup time_group = budget_key_time_frame_manager_log_1_0.time_group();
    execution_result = budget_key_timeframe_groups_->Erase(time_group);
    if (!execution_result.Successful()) {
      if (execution_result.status_code !=
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
        return execution_result;
      }
    }

    auto budget_key_timeframe_group =
        make_shared<BudgetKeyTimeframeGroup>(time_group);

    execution_result = Serialization::DeserializeBudgetKeyTimeframeGroupLog_1_0(
        budget_key_time_frame_manager_log_1_0.log_body(),
        budget_key_timeframe_group);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    auto budget_key_timeframe_group_pair =
        make_pair(time_group, budget_key_timeframe_group);
    execution_result = budget_key_timeframe_groups_->Insert(
        budget_key_timeframe_group_pair, budget_key_timeframe_group);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    budget_key_timeframe_group->needs_loader = false;
    budget_key_timeframe_group->is_loaded = true;
    return SuccessExecutionResult();
  }

  if (budget_key_time_frame_manager_log_1_0.operation_type() ==
      OperationType::REMOVE_TIMEGROUP_FROM_CACHE) {
    auto time_group = budget_key_time_frame_manager_log_1_0.time_group();
    execution_result = budget_key_timeframe_groups_->Erase(time_group);
    if (!execution_result.Successful()) {
      if (execution_result.status_code !=
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
        return execution_result;
      }
    }
    return SuccessExecutionResult();
  }

  if (budget_key_time_frame_manager_log_1_0.operation_type() ==
      OperationType::UPDATE_TIMEFRAME_RECORD) {
    shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
    execution_result = budget_key_timeframe_groups_->Find(
        budget_key_time_frame_manager_log_1_0.time_group(),
        budget_key_timeframe_group);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    execution_result = Serialization::DeserializeBudgetKeyTimeframeLog_1_0(
        budget_key_time_frame_manager_log_1_0.log_body(), budget_key_timeframe);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    auto time_bucket = budget_key_timeframe->time_bucket_index;
    execution_result =
        budget_key_timeframe_group->budget_key_timeframes.Erase(time_bucket);
    if (!execution_result.Successful()) {
      if (execution_result !=
          FailureExecutionResult(
              core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)) {
        return execution_result;
      }
    }

    auto pair = make_pair(budget_key_timeframe->time_bucket_index,
                          budget_key_timeframe);
    return budget_key_timeframe_group->budget_key_timeframes.Insert(
        pair, budget_key_timeframe);
  }

  if (budget_key_time_frame_manager_log_1_0.operation_type() ==
      OperationType::BATCH_UPDATE_TIMEFRAME_RECORDS_OF_TIMEGROUP) {
    shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
    execution_result = budget_key_timeframe_groups_->Find(
        budget_key_time_frame_manager_log_1_0.time_group(),
        budget_key_timeframe_group);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes;
    execution_result = Serialization::DeserializeBatchBudgetKeyTimeframeLog_1_0(
        budget_key_time_frame_manager_log_1_0.log_body(),
        budget_key_timeframes);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    if (budget_key_timeframes.empty()) {
      return FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
    }

    for (auto& budget_key_timeframe : budget_key_timeframes) {
      auto time_bucket = budget_key_timeframe->time_bucket_index;
      execution_result =
          budget_key_timeframe_group->budget_key_timeframes.Erase(time_bucket);
      if (!execution_result.Successful()) {
        if (execution_result !=
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)) {
          return execution_result;
        }
      }

      auto pair = make_pair(budget_key_timeframe->time_bucket_index,
                            budget_key_timeframe);
      execution_result =
          budget_key_timeframe_group->budget_key_timeframes.Insert(
              pair, budget_key_timeframe);
      if (!execution_result.Successful()) {
        return execution_result;
      }
    }
    return SuccessExecutionResult();
  }

  return FailureExecutionResult(
      core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG);
}

ExecutionResult BudgetKeyTimeframeManager::Checkpoint(
    shared_ptr<list<CheckpointLog>>& checkpoint_logs) noexcept {
  vector<TimeGroup> time_groups;
  auto execution_result = budget_key_timeframe_groups_->Keys(time_groups);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  for (auto time_group : time_groups) {
    shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
    execution_result = budget_key_timeframe_groups_->Find(
        time_group, budget_key_timeframe_group);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    CheckpointLog budget_key_timeframe_metadata_checkpoint_log;
    execution_result = Serialization::SerializeBudgetKeyTimeframeGroupLog(
        budget_key_timeframe_group,
        budget_key_timeframe_metadata_checkpoint_log.bytes_buffer);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    budget_key_timeframe_metadata_checkpoint_log.component_id = id_;
    budget_key_timeframe_metadata_checkpoint_log.log_id = Uuid::GenerateUuid();
    budget_key_timeframe_metadata_checkpoint_log.log_status =
        JournalLogStatus::Log;
    checkpoint_logs->push_back(
        move(budget_key_timeframe_metadata_checkpoint_log));
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::pbs
