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

#include "budget_key_provider.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/common/auto_expiry_concurrent_map/src/error_codes.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/journal_service_interface.h"
#include "pbs/budget_key/src/budget_key.h"
#include "pbs/budget_key/src/error_codes.h"
#include "pbs/budget_key_provider/src/proto/budget_key_provider.pb.h"
#include "pbs/interface/metrics_def.h"
#include "public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/src/metric_utils.h"
#include "public/cpio/utils/metric_aggregation/src/simple_metric.h"

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::CheckpointLog;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::JournalLogRequest;
using google::scp::core::JournalLogResponse;
using google::scp::core::JournalLogStatus;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Version;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Serialization;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::cpio::kCountUnit;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricLabelsBase;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::MetricUtils;
using google::scp::cpio::MetricValue;
using google::scp::pbs::budget_key_provider::proto::BudgetKeyProviderLog;
using google::scp::pbs::budget_key_provider::proto::BudgetKeyProviderLog_1_0;
using google::scp::pbs::budget_key_provider::proto::OperationType;
using std::bind;
using std::function;
using std::list;
using std::make_pair;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::this_thread::sleep_for;

static constexpr Version kCurrentVersion = {.major = 1, .minor = 0};
// This value MUST NOT change forever.
static Uuid kBudgetKeyProviderId = {.high = 0xFFFFFFF1, .low = 0x00000002};
static constexpr char kBudgetKeyProvider[] = "BudgetKeyProvider";

namespace google::scp::pbs {
ExecutionResult BudgetKeyProvider::Init() noexcept {
  size_t metric_aggregation_interval_milliseconds;
  if (!config_provider_
           ->Get(core::kAggregatedMetricIntervalMs,
                 metric_aggregation_interval_milliseconds)
           .Successful()) {
    metric_aggregation_interval_milliseconds =
        core::kDefaultAggregatedMetricIntervalMs;
  }

  // TODO: b/297077044 to avoid this. Construction of this should come from a
  // factory, and otherwise we cannot mock this causing tests to fail for any
  // changes to AggregateMetric class.
  budget_key_count_metric_ = MetricUtils::RegisterAggregateMetric(
      async_executor_, metric_client_, kMetricNameBudgetKeyCount,
      kMetricComponentNameAndPartitionNamePrefixForBudgetKey +
          ToString(partition_id_),
      kMetricMethodLoadUnload, kCountUnit,
      {kMetricEventLoadFromDBScheduled, kMetricEventLoadFromDBSuccess,
       kMetricEventLoadFromDBFailed, kMetricEventUnloadFromDBScheduled,
       kMetricEventUnloadFromDBSuccess, kMetricEventUnloadFromDBFailed},
      metric_aggregation_interval_milliseconds);
  RETURN_IF_FAILURE(budget_key_count_metric_->Init());

  return journal_service_->SubscribeForRecovery(
      kBudgetKeyProviderId,
      bind(&BudgetKeyProvider::OnJournalServiceRecoverCallback, this, _1, _2));
}

ExecutionResult BudgetKeyProvider::Run() noexcept {
  // TODO: b/297077044 to avoid this if case
  if (budget_key_count_metric_) {
    RETURN_IF_FAILURE(budget_key_count_metric_->Run());
  }

  // Before running the system, all the recovered budget keys must be reloaded.
  vector<string> keys;
  auto execution_result = budget_keys_->Keys(keys);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  for (auto key : keys) {
    shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair;
    execution_result = budget_keys_->Find(key, budget_key_provider_pair);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    auto execution_result = budget_key_provider_pair->budget_key->Run();
    if (execution_result.Successful()) {
      budget_key_provider_pair->needs_loader = false;
      budget_key_provider_pair->is_loaded = true;
      continue;
    }

    if (execution_result !=
        FailureExecutionResult(
            core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_NOT_INITIALIZED)) {
      return execution_result;
    }

    budget_key_provider_pair->is_loaded = false;
    budget_key_provider_pair->needs_loader = true;
  }

  // This line must be executed at the end to ensure keys will not be deleted
  // after the recovery.
  return budget_keys_->Run();
}

ExecutionResult BudgetKeyProvider::Stop() noexcept {
  RETURN_IF_FAILURE(budget_key_count_metric_->Stop());
  RETURN_IF_FAILURE(budget_keys_->Stop());

  vector<string> keys;
  auto execution_result = budget_keys_->Keys(keys);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  for (auto key : keys) {
    shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair;
    execution_result = budget_keys_->Find(key, budget_key_provider_pair);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    // If a budget key cannot be stopped, return error.
    auto execution_result = budget_key_provider_pair->budget_key->Stop();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}

BudgetKeyProvider::~BudgetKeyProvider() {
  if (journal_service_) {
    // Ignore the failure.
    journal_service_->UnsubscribeForRecovery(kBudgetKeyProviderId);
  }
}

ExecutionResult BudgetKeyProvider::SerializeBudgetKeyProviderPair(
    shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
    OperationType operation_type,
    BytesBuffer& budget_key_provider_log_bytes_buffer) noexcept {
  // Creating the budget key provider log object.
  BudgetKeyProviderLog budget_key_provider_log;
  budget_key_provider_log.mutable_version()->set_major(kCurrentVersion.major);
  budget_key_provider_log.mutable_version()->set_minor(kCurrentVersion.minor);

  // Creating the budget key provider log v1.0 object.
  BudgetKeyProviderLog_1_0 budget_key_provider_log_1_0;
  budget_key_provider_log_1_0.set_budget_key_name(
      *budget_key_provider_pair->budget_key->GetName());
  budget_key_provider_log_1_0.set_operation_type(operation_type);
  budget_key_provider_log_1_0.mutable_id()->set_high(
      budget_key_provider_pair->budget_key->GetId().high);
  budget_key_provider_log_1_0.mutable_id()->set_low(
      budget_key_provider_pair->budget_key->GetId().low);

  // Serialize the budget_key_provider_log_1_0 object.
  size_t offset = 0;
  size_t bytes_serialized = 0;
  BytesBuffer budget_key_provider_log_1_0_bytes_buffer(
      budget_key_provider_log_1_0.ByteSizeLong());
  auto execution_result =
      Serialization::SerializeProtoMessage<BudgetKeyProviderLog_1_0>(
          budget_key_provider_log_1_0_bytes_buffer, offset,
          budget_key_provider_log_1_0, bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  budget_key_provider_log_1_0_bytes_buffer.length = bytes_serialized;

  // Set the log body of the budget key provider log.
  budget_key_provider_log.set_log_body(
      budget_key_provider_log_1_0_bytes_buffer.bytes->data(),
      budget_key_provider_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  budget_key_provider_log_bytes_buffer.bytes =
      make_shared<vector<Byte>>(budget_key_provider_log.ByteSizeLong());
  budget_key_provider_log_bytes_buffer.capacity =
      budget_key_provider_log.ByteSizeLong();
  execution_result = Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
      budget_key_provider_log_bytes_buffer, offset, budget_key_provider_log,
      bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  budget_key_provider_log_bytes_buffer.length = bytes_serialized;
  return SuccessExecutionResult();
}

void BudgetKeyProvider::OnBeforeGarbageCollection(
    string& budget_key_name,
    shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
    function<void(bool)> should_delete_entry) noexcept {
  if (budget_key_provider_pair->budget_key->CanUnload() !=
      SuccessExecutionResult()) {
    should_delete_entry(false);
    return;
  }

  BytesBuffer budget_key_provider_log_bytes_buffer;
  auto execution_result = SerializeBudgetKeyProviderPair(
      budget_key_provider_pair, OperationType::DELETE_FROM_CACHE,
      budget_key_provider_log_bytes_buffer);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }

  auto activity_id = Uuid::GenerateUuid();
  auto budget_key_id_str =
      core::common::ToString(budget_key_provider_pair->budget_key->GetId());
  SCP_DEBUG(kBudgetKeyProvider, activity_id,
            "Unloading budget key name %s with id: %s",
            budget_key_provider_pair->budget_key->GetName()->c_str(),
            budget_key_id_str.c_str());

  // Sending to the journal service.
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.parent_activity_id = Uuid::GenerateUuid();
  journal_log_context.correlation_id = kBudgetKeyProviderId;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.request->component_id = kBudgetKeyProviderId;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data = make_shared<BytesBuffer>();
  journal_log_context.request->data->bytes =
      budget_key_provider_log_bytes_buffer.bytes;
  journal_log_context.request->data->length =
      budget_key_provider_log_bytes_buffer.length;
  journal_log_context.request->data->capacity =
      budget_key_provider_log_bytes_buffer.capacity;
  journal_log_context.callback =
      bind(&BudgetKeyProvider::OnRemoveEntryFromCacheLogged, this,
           should_delete_entry, budget_key_provider_pair, _1);

  // Request-level retry is not necessary here. If the request is unsuccessful,
  // retry in next round of OnBeforeGarbageCollection.
  execution_result = journal_service_->Log(journal_log_context);
  if (!execution_result.Successful()) {
    should_delete_entry(false);
    return;
  }
}

void BudgetKeyProvider::OnRemoveEntryFromCacheLogged(
    std::function<void(bool)> should_delete_entry,
    shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
    AsyncContext<JournalLogRequest, JournalLogResponse>&
        journal_log_context) noexcept {
  auto successful = false;
  if (journal_log_context.result.Successful()) {
    successful = true;
    auto execution_result = budget_key_provider_pair->budget_key->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR_CONTEXT(kBudgetKeyProvider, journal_log_context,
                        execution_result,
                        "Cannot stop the budget key before deletion.");
    }
  }

  should_delete_entry(successful);
}

ExecutionResult BudgetKeyProvider::OnJournalServiceRecoverCallback(
    const shared_ptr<BytesBuffer>& bytes_buffer,
    const Uuid& activity_id) noexcept {
  BudgetKeyProviderLog budget_key_provider_log;
  size_t offset = 0;
  size_t bytes_deserialized = 0;
  auto execution_result =
      Serialization::DeserializeProtoMessage<BudgetKeyProviderLog>(
          *bytes_buffer, offset, bytes_buffer->length, budget_key_provider_log,
          bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  execution_result =
      Serialization::ValidateVersion(budget_key_provider_log, kCurrentVersion);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  BudgetKeyProviderLog_1_0 budget_key_provider_log_1_0;
  bytes_deserialized = 0;
  execution_result =
      Serialization::DeserializeProtoMessage<BudgetKeyProviderLog_1_0>(
          budget_key_provider_log.log_body(), budget_key_provider_log_1_0,
          bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  auto budget_key_name =
      make_shared<BudgetKeyName>(budget_key_provider_log_1_0.budget_key_name());
  Uuid budget_key_id;
  budget_key_id.high = budget_key_provider_log_1_0.id().high();
  budget_key_id.low = budget_key_provider_log_1_0.id().low();

  auto budget_key_id_str = core::common::ToString(budget_key_id);
  SCP_DEBUG(kBudgetKeyProvider, activity_id, "Budget key recovered: %s %s.",
            budget_key_id_str.c_str(), budget_key_name->c_str());

  auto budget_key = make_shared<BudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_for_background_operations_,
      nosql_database_provider_for_live_traffic_, metric_client_,
      config_provider_, budget_key_count_metric_);

  if (budget_key_provider_log_1_0.operation_type() ==
      OperationType::LOAD_INTO_CACHE) {
    auto budget_key_provider_pair = make_shared<BudgetKeyProviderPair>();
    budget_key_provider_pair->budget_key = budget_key;
    budget_key_provider_pair->is_loaded = false;

    auto budget_key_pair =
        make_pair(budget_key_provider_log_1_0.budget_key_name(),
                  budget_key_provider_pair);
    auto insertion_result =
        budget_keys_->Insert(budget_key_pair, budget_key_provider_pair);
    if (!insertion_result.Successful()) {
      if (budget_key_provider_pair->budget_key->GetId() !=
          budget_key->GetId()) {
        return insertion_result;
      }

      budget_key_provider_pair->budget_key = budget_key;
    }

    return budget_key->Init();
  }

  if (budget_key_provider_log_1_0.operation_type() ==
      OperationType::DELETE_FROM_CACHE) {
    shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair;
    execution_result =
        budget_keys_->Find(*budget_key_name, budget_key_provider_pair);

    if (!execution_result.Successful()) {
      if (execution_result.status_code !=
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
        return execution_result;
      }

      return SuccessExecutionResult();
    }

    return budget_keys_->Erase(*budget_key_name);
  }

  return FailureExecutionResult(
      core::errors::SC_BUDGET_KEY_PROVIDER_INVALID_OPERATION_TYPE);
}

ExecutionResult BudgetKeyProvider::GetBudgetKey(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context) noexcept {
  Uuid key_id = Uuid::GenerateUuid();

  // To avoid locking, we use concurrent map to handle concurrency.
  auto budget_key_provider_pair = make_shared<BudgetKeyProviderPair>();
  budget_key_provider_pair->budget_key = make_shared<BudgetKey>(
      get_budget_key_context.request->budget_key_name, key_id, async_executor_,
      journal_service_, nosql_database_provider_for_background_operations_,
      nosql_database_provider_for_live_traffic_, metric_client_,
      config_provider_, budget_key_count_metric_);

  auto budget_key_pair =
      make_pair(*get_budget_key_context.request->budget_key_name,
                budget_key_provider_pair);

  // Regardless of outcome we will insert into the map. The outcome can be
  // success or failure but in both cases the element will be in the map.
  auto execution_result =
      budget_keys_->Insert(budget_key_pair, budget_key_provider_pair);

  // The entry already exists, therefore budget_key will point to the previously
  // inserted object.
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
    if (budget_key_provider_pair->needs_loader) {
      auto needs_loader = true;
      if (budget_key_provider_pair->needs_loader.compare_exchange_strong(
              needs_loader, false)) {
        should_load = true;
      }
    }

    if (!should_load) {
      if (!budget_key_provider_pair->is_loaded) {
        return RetryExecutionResult(
            core::errors::SC_BUDGET_KEY_PROVIDER_ENTRY_IS_LOADING);
      }

      GetBudgetKeyResponse get_budget_key_response{
          .budget_key = budget_key_provider_pair->budget_key};
      get_budget_key_context.response =
          make_shared<GetBudgetKeyResponse>(move(get_budget_key_response));
      get_budget_key_context.result = SuccessExecutionResult();
      get_budget_key_context.Finish();
      return SuccessExecutionResult();
    }
  }

  execution_result = budget_keys_->DisableEviction(budget_key_pair.first);
  if (!execution_result.Successful()) {
    return RetryExecutionResult(execution_result.status_code);
  }

  auto budget_key_id_str = core::common::ToString(key_id);
  SCP_DEBUG_CONTEXT(kBudgetKeyProvider, get_budget_key_context,
                    "Loading budget key name %s with id: %s",
                    get_budget_key_context.request->budget_key_name->c_str(),
                    budget_key_id_str.c_str());

  return LogLoadBudgetKeyIntoCache(get_budget_key_context,
                                   budget_key_provider_pair);
};

ExecutionResult BudgetKeyProvider::LogLoadBudgetKeyIntoCache(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair) noexcept {
  BytesBuffer budget_key_provider_log_bytes_buffer;
  auto execution_result = SerializeBudgetKeyProviderPair(
      budget_key_provider_pair, OperationType::LOAD_INTO_CACHE,
      budget_key_provider_log_bytes_buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Sending to the journal service.
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.parent_activity_id = get_budget_key_context.activity_id;
  journal_log_context.correlation_id = get_budget_key_context.correlation_id;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.request->component_id = kBudgetKeyProviderId;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data = make_shared<BytesBuffer>();
  journal_log_context.request->data->bytes =
      budget_key_provider_log_bytes_buffer.bytes;
  journal_log_context.request->data->length =
      budget_key_provider_log_bytes_buffer.length;
  journal_log_context.request->data->capacity =
      budget_key_provider_log_bytes_buffer.capacity;
  journal_log_context.callback =
      bind(&BudgetKeyProvider::OnLogLoadBudgetKeyIntoCacheCallback, this,
           get_budget_key_context, budget_key_provider_pair, _1);

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

void BudgetKeyProvider::OnLogLoadBudgetKeyIntoCacheCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
    AsyncContext<JournalLogRequest, JournalLogResponse>&
        journal_log_context) noexcept {
  if (!journal_log_context.result.Successful()) {
    budget_key_provider_pair->needs_loader = true;
    auto execution_result = budget_keys_->EnableEviction(
        *budget_key_provider_pair->budget_key->GetName());
    if (!execution_result.Successful()) {
      SCP_ERROR_CONTEXT(
          kBudgetKeyProvider, get_budget_key_context, execution_result,
          "Cache eviction failed for %s",
          budget_key_provider_pair->budget_key->GetName()->c_str());
    }

    get_budget_key_context.result = journal_log_context.result;
    get_budget_key_context.Finish();
    return;
  }

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;
  load_budget_key_context.parent_activity_id =
      get_budget_key_context.activity_id;
  load_budget_key_context.correlation_id =
      get_budget_key_context.correlation_id;
  load_budget_key_context.callback =
      bind(&BudgetKeyProvider::OnLoadBudgetKeyCallback, this,
           get_budget_key_context, budget_key_provider_pair, _1);

  operation_dispatcher_
      .Dispatch<AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>>(
          load_budget_key_context,
          [budget_key_provider_pair](
              AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
                  load_budget_key_context) {
            return budget_key_provider_pair->budget_key->LoadBudgetKey(
                load_budget_key_context);
          });
}

void BudgetKeyProvider::OnLoadBudgetKeyCallback(
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
        get_budget_key_context,
    shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair,
    AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
        load_budget_key_context) noexcept {
  auto execution_result = budget_keys_->EnableEviction(
      *budget_key_provider_pair->budget_key->GetName());
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kBudgetKeyProvider, get_budget_key_context,
                      execution_result, "Cache eviction failed for %s",
                      budget_key_provider_pair->budget_key->GetName()->c_str());
  }

  if (!load_budget_key_context.result.Successful()) {
    budget_key_provider_pair->needs_loader = true;
    get_budget_key_context.result = load_budget_key_context.result;
    get_budget_key_context.Finish();
    return;
  }

  execution_result = budget_key_provider_pair->budget_key->Run();
  if (!execution_result.Successful()) {
    budget_key_provider_pair->needs_loader = true;
    get_budget_key_context.result = execution_result;
    get_budget_key_context.Finish();
    return;
  }

  budget_key_provider_pair->is_loaded = true;
  GetBudgetKeyResponse get_budget_key_response{
      .budget_key = budget_key_provider_pair->budget_key};
  get_budget_key_context.response =
      make_shared<GetBudgetKeyResponse>(move(get_budget_key_response));
  get_budget_key_context.result = SuccessExecutionResult();
  get_budget_key_context.Finish();
}

ExecutionResult BudgetKeyProvider::Checkpoint(
    shared_ptr<list<CheckpointLog>>& checkpoint_logs) noexcept {
  vector<string> budget_keys;
  auto execution_result = budget_keys_->Keys(budget_keys);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_INFO(kBudgetKeyProvider, activity_id_,
           "Number of active budget keys in map to checkpoint: %llu",
           budget_keys.size());
  for (auto budget_key : budget_keys) {
    shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair;
    execution_result = budget_keys_->Find(budget_key, budget_key_provider_pair);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    CheckpointLog budget_key_checkpoint_log;
    execution_result = SerializeBudgetKeyProviderPair(
        budget_key_provider_pair, OperationType::LOAD_INTO_CACHE,
        budget_key_checkpoint_log.bytes_buffer);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    budget_key_checkpoint_log.component_id = kBudgetKeyProviderId;
    budget_key_checkpoint_log.log_id = Uuid::GenerateUuid();
    budget_key_checkpoint_log.log_status = JournalLogStatus::Log;

    checkpoint_logs->push_back(move(budget_key_checkpoint_log));
  }

  for (auto budget_key : budget_keys) {
    shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair;
    execution_result = budget_keys_->Find(budget_key, budget_key_provider_pair);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    execution_result =
        budget_key_provider_pair->budget_key->Checkpoint(checkpoint_logs);
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::pbs
