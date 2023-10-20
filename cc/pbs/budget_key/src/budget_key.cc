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

#include "budget_key.h"

#include <functional>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "core/common/serialization/src/serialization.h"
#include "core/common/uuid/src/uuid.h"
#include "pbs/budget_key/src/proto/budget_key.pb.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_manager.h"
#include "pbs/budget_key_transaction_protocols/src/consume_budget_transaction_protocol.h"

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::CheckpointLog;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::JournalLogRequest;
using google::scp::core::JournalLogResponse;
using google::scp::core::JournalLogStatus;
using google::scp::core::JournalServiceInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Version;
using google::scp::core::common::Serialization;
using google::scp::core::common::Uuid;
using google::scp::cpio::MetricClientInterface;
using google::scp::pbs::budget_key::proto::BudgetKeyLog;
using google::scp::pbs::budget_key::proto::BudgetKeyLog_1_0;
using std::bind;
using std::list;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;

static constexpr Version kCurrentVersion = {.major = 1, .minor = 0};
static constexpr char kBudgetKey[] = "BudgetKey";

namespace google::scp::pbs {
BudgetKey::BudgetKey(
    const shared_ptr<BudgetKeyName>& name, const Uuid& id,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<JournalServiceInterface>& journal_service,
    const shared_ptr<NoSQLDatabaseProviderInterface>&
        nosql_database_provider_for_background_operations,
    const shared_ptr<NoSQLDatabaseProviderInterface>&
        nosql_database_provider_for_live_traffic,
    const shared_ptr<MetricClientInterface>& metric_client,
    const shared_ptr<ConfigProviderInterface>& config_provider,
    const shared_ptr<cpio::AggregateMetricInterface>& budget_key_count_metric)
    : name_(name),
      id_(id),
      async_executor_(async_executor),
      journal_service_(journal_service),
      nosql_database_provider_for_background_operations_(
          nosql_database_provider_for_background_operations),
      nosql_database_provider_for_live_traffic_(
          nosql_database_provider_for_live_traffic),
      operation_dispatcher_(async_executor,
                            core::common::RetryStrategy(
                                core::common::RetryStrategyType::Exponential,
                                kBudgetKeyRetryStrategyDelayMs,
                                kBudgetKeyRetryStrategyTotalRetries)),
      metric_client_(metric_client),
      config_provider_(config_provider),
      budget_key_count_metric_(budget_key_count_metric) {}

ExecutionResult BudgetKey::Init() noexcept {
  return journal_service_->SubscribeForRecovery(
      id_, bind(&BudgetKey::OnJournalServiceRecoverCallback, this, _1, _2));
}

ExecutionResult BudgetKey::Run() noexcept {
  if (!budget_key_timeframe_manager_) {
    return FailureExecutionResult(
        core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_NOT_INITIALIZED);
  }

  return budget_key_timeframe_manager_->Run();
}

ExecutionResult BudgetKey::Stop() noexcept {
  if (budget_key_timeframe_manager_) {
    return budget_key_timeframe_manager_->Stop();
  }
  return SuccessExecutionResult();
}

BudgetKey::~BudgetKey() {
  if (journal_service_) {
    // Ignore the failure.
    journal_service_->UnsubscribeForRecovery(id_);
  }
}

Uuid BudgetKey::GetTimeframeManagerId() noexcept {
  Uuid timeframe_manager_id;
  timeframe_manager_id.high = ~id_.high;
  timeframe_manager_id.low = ~id_.low;
  return timeframe_manager_id;
}

ExecutionResult BudgetKey::OnJournalServiceRecoverCallback(
    const shared_ptr<BytesBuffer>& bytes_buffer,
    const Uuid& activity_id) noexcept {
  SCP_DEBUG(
      kBudgetKey, activity_id,
      "Recovering budget key from the stored logs. The current bytes size: "
      "%zu.",
      bytes_buffer->length);

  BudgetKeyLog budget_key_log;
  size_t bytes_deserialized = 0;
  size_t offset = 0;
  auto execution_result = Serialization::DeserializeProtoMessage<BudgetKeyLog>(
      *bytes_buffer, offset, bytes_buffer->length, budget_key_log,
      bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  offset += bytes_deserialized;

  execution_result =
      Serialization::ValidateVersion(budget_key_log, kCurrentVersion);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  BudgetKeyLog_1_0 budget_key_log_1_0;
  execution_result = Serialization::DeserializeProtoMessage<BudgetKeyLog_1_0>(
      budget_key_log.log_body(), budget_key_log_1_0, bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  Uuid timeframe_manager_id;
  timeframe_manager_id.high = budget_key_log_1_0.timeframe_manager_id().high();
  timeframe_manager_id.low = budget_key_log_1_0.timeframe_manager_id().low();

  auto budget_key_id = core::common::ToString(id_);
  auto timeframe_manager_id_str = core::common::ToString(timeframe_manager_id);
  SCP_DEBUG(kBudgetKey, activity_id,
            "Budget key %s Timeframe manager recovered: "
            "%s.",
            budget_key_id.c_str(), timeframe_manager_id_str.c_str());

  budget_key_timeframe_manager_ = make_shared<BudgetKeyTimeframeManager>(
      name_, timeframe_manager_id, async_executor_, journal_service_,
      nosql_database_provider_for_background_operations_,
      nosql_database_provider_for_live_traffic_, metric_client_,
      config_provider_, budget_key_count_metric_);
  consume_budget_transaction_protocol_ =
      make_shared<ConsumeBudgetTransactionProtocol>(
          budget_key_timeframe_manager_);

  return budget_key_timeframe_manager_->Init();
}

ExecutionResult BudgetKey::SerializeBudgetKey(
    Uuid& budget_key_timeframe_manager_id,
    core::BytesBuffer& budget_key_log_bytes_buffer) noexcept {
  // Creating log object
  BudgetKeyLog budget_key_log;
  budget_key_log.mutable_version()->set_major(kCurrentVersion.major);
  budget_key_log.mutable_version()->set_minor(kCurrentVersion.minor);

  // Creating log v1.0 object.
  BudgetKeyLog_1_0 budget_key_log_1_0;
  budget_key_log_1_0.mutable_timeframe_manager_id()->set_high(
      budget_key_timeframe_manager_id.high);
  budget_key_log_1_0.mutable_timeframe_manager_id()->set_low(
      budget_key_timeframe_manager_id.low);

  // Serializing the log v1.0 object.
  size_t bytes_serialized = 0;
  BytesBuffer budget_key_log_1_0_bytes_buffer(
      budget_key_log_1_0.ByteSizeLong());
  auto execution_result =
      Serialization::SerializeProtoMessage<BudgetKeyLog_1_0>(
          budget_key_log_1_0_bytes_buffer, 0, budget_key_log_1_0,
          bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  budget_key_log_1_0_bytes_buffer.length = bytes_serialized;

  // Setting the serialized log to the budget key log_body.
  budget_key_log.set_log_body(budget_key_log_1_0_bytes_buffer.bytes->data(),
                              budget_key_log_1_0_bytes_buffer.length);

  // Serializing the log object.
  bytes_serialized = 0;
  budget_key_log_bytes_buffer.bytes =
      make_shared<vector<Byte>>(budget_key_log.ByteSizeLong());
  budget_key_log_bytes_buffer.capacity = budget_key_log.ByteSizeLong();
  execution_result = Serialization::SerializeProtoMessage<BudgetKeyLog>(
      budget_key_log_bytes_buffer, 0, budget_key_log, bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  budget_key_log_bytes_buffer.length = bytes_serialized;
  return SuccessExecutionResult();
}

ExecutionResult BudgetKey::CanUnload() noexcept {
  if (budget_key_timeframe_manager_) {
    return budget_key_timeframe_manager_->CanUnload();
  }
  return SuccessExecutionResult();
};

ExecutionResult BudgetKey::LoadBudgetKey(
    AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
        load_budget_key_context) noexcept {
  Uuid timeframe_manager_id = GetTimeframeManagerId();
  BytesBuffer budget_key_log_bytes_buffer;
  auto execution_result =
      SerializeBudgetKey(timeframe_manager_id, budget_key_log_bytes_buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  auto budget_key_id = core::common::ToString(id_);
  auto budget_key_timeframe_manager_id =
      core::common::ToString(timeframe_manager_id);
  SCP_DEBUG_CONTEXT(
      kBudgetKey, load_budget_key_context,
      "Loading budget key with id: %s and timeframe manager id: %s",
      budget_key_id.c_str(), budget_key_timeframe_manager_id.c_str());

  // Sending log request to the journal service.
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.parent_activity_id = load_budget_key_context.activity_id;
  journal_log_context.correlation_id = load_budget_key_context.correlation_id;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.request->component_id = id_;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data = make_shared<BytesBuffer>();
  journal_log_context.request->data->bytes = budget_key_log_bytes_buffer.bytes;
  journal_log_context.request->data->length =
      budget_key_log_bytes_buffer.length;
  journal_log_context.request->data->capacity =
      budget_key_log_bytes_buffer.capacity;
  journal_log_context.callback =
      bind(&BudgetKey::OnLogLoadBudgetKeyCallback, this,
           load_budget_key_context, timeframe_manager_id, _1);

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

void BudgetKey::OnLogLoadBudgetKeyCallback(
    core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
        load_budget_key_context,
    core::common::Uuid& budget_key_timeframe_manager_id,
    core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>&
        journal_log_context) noexcept {
  // Check if the journaling operation has been successful.
  if (!journal_log_context.result.Successful()) {
    load_budget_key_context.result = journal_log_context.result;
    load_budget_key_context.Finish();
    return;
  }

  // Construct the budget key timeframe manager with id.
  budget_key_timeframe_manager_ = make_shared<BudgetKeyTimeframeManager>(
      name_, budget_key_timeframe_manager_id, async_executor_, journal_service_,
      nosql_database_provider_for_background_operations_,
      nosql_database_provider_for_live_traffic_, metric_client_,
      config_provider_, budget_key_count_metric_);
  consume_budget_transaction_protocol_ =
      make_shared<ConsumeBudgetTransactionProtocol>(
          budget_key_timeframe_manager_);

  load_budget_key_context.result = SuccessExecutionResult();
  load_budget_key_context.Finish();
}

ExecutionResult BudgetKey::GetBudget(
    AsyncContext<GetBudgetRequest, GetBudgetResponse>&
        get_budget_context) noexcept {
  LoadBudgetKeyTimeframeRequest request = {
      .reporting_times = {get_budget_context.request->time_bucket}};

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(move(request)),
          bind(&BudgetKey::OnBudgetLoaded, this, get_budget_context, _1),
          get_budget_context);

  operation_dispatcher_.Dispatch<AsyncContext<LoadBudgetKeyTimeframeRequest,
                                              LoadBudgetKeyTimeframeResponse>>(
      load_budget_key_timeframe_context,
      [budget_key_timeframe_manager = budget_key_timeframe_manager_](
          AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>&
              load_budget_key_timeframe_context) {
        return budget_key_timeframe_manager->Load(
            load_budget_key_timeframe_context);
      });

  return SuccessExecutionResult();
}

void BudgetKey::OnBudgetLoaded(
    AsyncContext<GetBudgetRequest, GetBudgetResponse>& get_budget_context,
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>&
        load_budget_key_timeframe_context) noexcept {
  if (!load_budget_key_timeframe_context.result.Successful()) {
    get_budget_context.result = load_budget_key_timeframe_context.result;
    get_budget_context.Finish();
    return;
  }

  GetBudgetResponse response{.token_count = kMaxToken};
  get_budget_context.response = make_shared<GetBudgetResponse>(move(response));
  get_budget_context.result = load_budget_key_timeframe_context.result;
  get_budget_context.Finish();
}

ExecutionResult BudgetKey::Checkpoint(
    shared_ptr<list<CheckpointLog>>& checkpoint_logs) noexcept {
  Uuid timeframe_manager_id = GetTimeframeManagerId();
  CheckpointLog budget_key_checkpoint_log;
  auto execution_result = SerializeBudgetKey(
      timeframe_manager_id, budget_key_checkpoint_log.bytes_buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  budget_key_checkpoint_log.component_id = id_;
  budget_key_checkpoint_log.log_id = Uuid::GenerateUuid();
  budget_key_checkpoint_log.log_status = JournalLogStatus::Log;
  checkpoint_logs->push_back(budget_key_checkpoint_log);

  if (budget_key_timeframe_manager_) {
    return budget_key_timeframe_manager_->Checkpoint(checkpoint_logs);
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::pbs
