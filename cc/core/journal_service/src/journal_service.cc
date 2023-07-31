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

#include "journal_service.h"

#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/interface/configuration_keys.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/metric_client_provider/interface/simple_metric_interface.h"
#include "cpio/client_providers/metric_client_provider/src/utils/simple_metric.h"

#include "error_codes.h"
#include "journal_input_stream.h"

using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalStreamAppendLogRequest;
using google::scp::core::journal_service::JournalStreamAppendLogResponse;
using google::scp::core::journal_service::JournalStreamReadLogRequest;
using google::scp::core::journal_service::JournalStreamReadLogResponse;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::MetricValue;
using google::scp::cpio::client_providers::kCountUnit;
using google::scp::cpio::client_providers::kMillisecondsUnit;
using google::scp::cpio::client_providers::MetricDefinition;
using google::scp::cpio::client_providers::MetricLabelsBase;
using google::scp::cpio::client_providers::SimpleMetric;
using google::scp::cpio::client_providers::SimpleMetricInterface;
using google::scp::cpio::client_providers::TimeEvent;
using std::atomic;
using std::bind;
using std::function;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::unordered_set;
using std::vector;
using std::chrono::milliseconds;
using std::placeholders::_1;
using std::this_thread::sleep_for;

static constexpr size_t kMaxWaitTimeForFlushMs = 20;
static constexpr size_t kStartupWaitIntervalMilliseconds = 100;
static constexpr char kJournalService[] = "JournalService";
static constexpr char kRecoverSimpleMetricName[] = "RecoverExecutionTime";
static constexpr char kRecoverMethod[] = "Recover";

namespace google::scp::core {

ExecutionResult JournalService::RegisterSimpleMetric(
    shared_ptr<SimpleMetricInterface>& metrics_instance,
    const string& name) noexcept {
  auto metric_name = make_shared<MetricName>(name);
  auto metric_unit = make_shared<MetricUnit>(kMillisecondsUnit);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(kJournalService, kRecoverMethod);
  metric_info->labels =
      make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  metrics_instance =
      make_shared<SimpleMetric>(async_executor_, metric_client_, metric_info);

  return SuccessExecutionResult();
}

ExecutionResult JournalService::Init() noexcept {
  if (is_initialized_) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_ALREADY_INITIALIZED);
  }
  is_initialized_ = true;

  auto execution_result = blob_storage_provider_->CreateBlobStorageClient(
      blob_storage_provider_client_);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  journal_input_stream_ = make_shared<JournalInputStream>(
      bucket_name_, partition_name_, blob_storage_provider_client_);

  RegisterSimpleMetric(recover_time_metrics_, kRecoverSimpleMetricName);
  execution_result = recover_time_metrics_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (!config_provider_
           ->Get(kPBSJournalServiceFlushIntervalInMilliseconds,
                 journal_flush_interval_in_milliseconds_)
           .Successful()) {
    journal_flush_interval_in_milliseconds_ = kMaxWaitTimeForFlushMs;
  }

  INFO(kJournalService, kZeroUuid, kZeroUuid,
       "Starting Journal Service. Flush interval %zu milliseconds",
       journal_flush_interval_in_milliseconds_);

  return SuccessExecutionResult();
}

ExecutionResult JournalService::Run() noexcept {
  if (!is_initialized_) {
    return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_NOT_INITIALIZED);
  }

  if (is_running_) {
    return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_RUNNING);
  }

  is_running_ = true;
  auto execution_result = recover_time_metrics_->Run();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  atomic<bool> flushing_thread_started(false);
  flushing_thread_ = make_unique<thread>([this, &flushing_thread_started]() {
    flushing_thread_started = true;
    FlushJournalOutputStream();
  });

  while (!flushing_thread_started) {
    sleep_for(milliseconds(kStartupWaitIntervalMilliseconds));
  }

  return SuccessExecutionResult();
}

ExecutionResult JournalService::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_STOPPED);
  }

  is_running_ = false;

  auto execution_result = recover_time_metrics_->Stop();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (flushing_thread_->joinable()) {
    flushing_thread_->join();
  }

  return SuccessExecutionResult();
}

ExecutionResult JournalService::Recover(
    AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
        journal_recover_context) noexcept {
  shared_ptr<TimeEvent> time_event = make_shared<TimeEvent>();
  auto replayed_log_ids = make_shared<unordered_set<string>>();
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context(
          make_shared<JournalStreamReadLogRequest>(),
          bind(&JournalService::OnJournalStreamReadLogCallback, this,
               time_event, replayed_log_ids, journal_recover_context, _1),
          journal_recover_context.activity_id);
  journal_stream_read_log_context.request->max_journal_id_to_process =
      journal_recover_context.request->max_journal_id_to_process;
  journal_stream_read_log_context.request->max_number_of_journals_to_process =
      journal_recover_context.request->max_number_of_journals_to_process;

  return journal_input_stream_->ReadLog(journal_stream_read_log_context);
}

void JournalService::OnJournalStreamReadLogCallback(
    shared_ptr<TimeEvent>& time_event,
    shared_ptr<unordered_set<string>>& replayed_log_ids,
    AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
        journal_recover_context,
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context) noexcept {
  if (!journal_stream_read_log_context.result.Successful()) {
    journal_recover_context.result = SuccessExecutionResult();
    if (journal_stream_read_log_context.result !=
        FailureExecutionResult(
            errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN)) {
      journal_recover_context.result = journal_stream_read_log_context.result;
    } else {
      time_event->Stop();
      recover_time_metrics_->Push(
          make_shared<MetricValue>(to_string(time_event->diff_time)));
      journal_recover_context.response = make_shared<JournalRecoverResponse>();
      journal_recover_context.response->last_processed_journal_id =
          journal_input_stream_->GetLastProcessedJournalId();
      journal_output_stream_ = make_shared<JournalOutputStream>(
          bucket_name_, partition_name_, async_executor_,
          blob_storage_provider_client_);
      // Set to nullptr to deallocate the stream and its data.
      journal_input_stream_ = nullptr;
    }
    journal_recover_context.Finish();
    return;
  }

  for (const auto& log : *journal_stream_read_log_context.response->read_logs) {
    function<ExecutionResult(const shared_ptr<BytesBuffer>&)> callback;
    auto pair = make_pair(log.component_id, callback);
    auto execution_result = subscribers_map_.Find(log.component_id, callback);
    auto component_id_str = core::common::ToString(log.component_id);
    if (!execution_result.Successful()) {
      ERROR_CONTEXT(kJournalService, journal_recover_context, execution_result,
                    "Cannot find the component with id %s",
                    component_id_str.c_str());

      journal_recover_context.result = execution_result;
      journal_recover_context.Finish();
      return;
    }

    auto log_id_str = core::common::ToString(log.log_id);

    // Check to see if the logs has been already replayed. There is always a
    // chance that a retry call makes the same log again.
    auto log_index = absl::StrCat(component_id_str, "_", log_id_str);
    if (replayed_log_ids->find(log_index) != replayed_log_ids->end()) {
      DEBUG_CONTEXT(kJournalService, journal_recover_context,
                    "Duplicate log id: %s.", log_index.c_str());
      continue;
    }

    replayed_log_ids->emplace(log_index);

    auto bytes_buffer = make_shared<BytesBuffer>(log.journal_log->log_body());
    execution_result = callback(bytes_buffer);
    if (!execution_result.Successful()) {
      ERROR_CONTEXT(
          kJournalService, journal_recover_context, execution_result,
          "Cannot handle the journal log with id %s for component id %s. "
          "Checkpoint/Journal ID where this came from: %llu",
          ToString(log.log_id).c_str(), component_id_str.c_str(),
          log.journal_id);

      journal_recover_context.result = execution_result;
      journal_recover_context.Finish();
      return;
    }
  }

  // There might be lots of logs to recover, there need to be a mechanism to
  // reduce the call stack size. Currently there is 1MB max stack limitation
  // that needed to be avoided.
  async_executor_->Schedule(
      [operation_dispatcher = &operation_dispatcher_,
       journal_stream_read_log_context,
       journal_input_stream = journal_input_stream_]() mutable {
        operation_dispatcher->Dispatch<AsyncContext<
            JournalStreamReadLogRequest, JournalStreamReadLogResponse>>(
            journal_stream_read_log_context,
            [journal_input_stream](AsyncContext<JournalStreamReadLogRequest,
                                                JournalStreamReadLogResponse>&
                                       journal_stream_read_log_context) {
              return journal_input_stream->ReadLog(
                  journal_stream_read_log_context);
            });
      },
      AsyncPriority::Urgent);
}

ExecutionResult JournalService::Log(
    AsyncContext<JournalLogRequest, JournalLogResponse>&
        journal_log_context) noexcept {
  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context(
          make_shared<JournalStreamAppendLogRequest>(),
          bind(&JournalService::OnJournalStreamAppendLogCallback, this,
               journal_log_context, _1),
          journal_log_context.activity_id);
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();
  journal_stream_append_log_context.request->journal_log->set_log_body(
      journal_log_context.request->data->bytes->data(),
      journal_log_context.request->data->length);
  journal_stream_append_log_context.request->component_id =
      journal_log_context.request->component_id;
  journal_stream_append_log_context.request->log_id =
      journal_log_context.request->log_id;
  journal_stream_append_log_context.request->log_status =
      journal_log_context.request->log_status;

  return journal_output_stream_->AppendLog(journal_stream_append_log_context);
}

void JournalService::OnJournalStreamAppendLogCallback(
    AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
    AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>&
        journal_stream_append_log_context) noexcept {
  journal_log_context.result = journal_stream_append_log_context.result;
  journal_log_context.Finish();
}

ExecutionResult JournalService::SubscribeForRecovery(
    const Uuid& component_id,
    function<ExecutionResult(const shared_ptr<BytesBuffer>&)>
        callback) noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_CANNOT_SUBSCRIBE_WHEN_RUNNING);
  }

  auto pair = make_pair(component_id, callback);
  auto execution_result = subscribers_map_.Insert(pair, callback);
  return execution_result;
}

ExecutionResult JournalService::UnsubscribeForRecovery(
    const Uuid& component_id) noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_CANNOT_UNSUBSCRIBE_WHEN_RUNNING);
  }

  auto id = component_id;
  return subscribers_map_.Erase(id);
}

ExecutionResult JournalService::GetLastPersistedJournalId(
    JournalId& journal_id) noexcept {
  return journal_output_stream_->GetLastPersistedJournalId(journal_id);
}

void JournalService::FlushJournalOutputStream() noexcept {
  while (is_running_) {
    if (journal_output_stream_) {
      while (!journal_output_stream_->FlushLogs().Successful()) {}
    }

    sleep_for(milliseconds(journal_flush_interval_in_milliseconds_));
  }
}

}  // namespace google::scp::core
