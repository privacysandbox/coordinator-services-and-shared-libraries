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

#include "transaction_manager.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/interface/configuration_keys.h"
#include "core/interface/metrics_def.h"
#include "core/interface/partition_types.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/src/aggregate_metric.h"

#include "error_codes.h"
#include "transaction_engine.h"

using google::scp::cpio::AggregateMetric;
using google::scp::cpio::AggregateMetricInterface;
using google::scp::cpio::kCountSecond;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricLabelsBase;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using std::atomic;
using std::function;
using std::list;
using std::make_shared;
using std::make_unique;
using std::mutex;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

static constexpr size_t kShutdownWaitIntervalMilliseconds = 100;

static constexpr char kTransactionManager[] = "TransactionManager";

namespace google::scp::core {
ExecutionResult TransactionManager::RegisterAggregateMetric(
    shared_ptr<AggregateMetricInterface>& metrics_instance,
    const string& name) noexcept {
  auto metric_name = make_shared<MetricName>(name);
  auto metric_unit = make_shared<MetricUnit>(kCountSecond);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(
      kMetricComponentNameAndPartitionNamePrefixForTransactionManager +
          ToString(partition_id_),
      name);
  metric_info->labels =
      make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  vector<string> event_list = {kMetricEventReceivedTransaction,
                               kMetricEventFinishedTransaction};
  metrics_instance = make_shared<AggregateMetric>(
      async_executor_, metric_client_, metric_info,
      aggregated_metric_interval_ms_, make_shared<vector<string>>(event_list));

  return SuccessExecutionResult();
}

TransactionManager::TransactionManager(
    shared_ptr<AsyncExecutorInterface>& async_executor,
    shared_ptr<TransactionCommandSerializerInterface>&
        transaction_command_serializer,
    shared_ptr<JournalServiceInterface>& journal_service,
    shared_ptr<RemoteTransactionManagerInterface>& remote_transaction_manager,
    size_t max_concurrent_transactions,
    const shared_ptr<MetricClientInterface>& metric_client,
    shared_ptr<ConfigProviderInterface> config_provider,
    const PartitionId& partition_id)
    : TransactionManager(
          async_executor,
          make_shared<TransactionEngine>(
              async_executor, transaction_command_serializer, journal_service,
              remote_transaction_manager, metric_client, config_provider),
          max_concurrent_transactions, metric_client, config_provider,
          partition_id) {}

ExecutionResult TransactionManager::Init() noexcept {
  if (max_concurrent_transactions_ <= 0) {
    return FailureExecutionResult(
        errors::
            SC_TRANSACTION_MANAGER_INVALID_MAX_CONCURRENT_TRANSACTIONS_VALUE);
  }

  if (started_) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_ALREADY_STARTED);
  }

  if (!config_provider_ ||
      !config_provider_
           ->Get(kAggregatedMetricIntervalMs, aggregated_metric_interval_ms_)
           .Successful()) {
    aggregated_metric_interval_ms_ = kDefaultAggregatedMetricIntervalMs;
  }

  RegisterAggregateMetric(active_transactions_metric_,
                          kMetricNameActiveTransaction);
  auto execution_result = active_transactions_metric_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return transaction_engine_->Init();
}

ExecutionResult TransactionManager::Run() noexcept {
  if (started_) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_ALREADY_STARTED);
  }

  started_ = true;

  auto execution_result = transaction_engine_->Run();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = active_transactions_metric_->Run();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult TransactionManager::Stop() noexcept {
  if (!started_) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_ALREADY_STOPPED);
  }

  started_ = false;

  while (active_transactions_count_ > 0) {
    SCP_INFO(kTransactionManager, activity_id_,
             "Waiting for '%llu' active transactions to exit...",
             active_transactions_count_.load());
    // The wait value can be any.
    sleep_for(milliseconds(kShutdownWaitIntervalMilliseconds));
  }

  auto execution_result = active_transactions_metric_->Stop();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return transaction_engine_->Stop();
}

ExecutionResult TransactionManager::Execute(
    AsyncContext<TransactionRequest, TransactionResponse>&
        transaction_context) noexcept {
  if (active_transactions_count_ >= max_concurrent_transactions_) {
    return RetryExecutionResult(
        errors::SC_TRANSACTION_MANAGER_CANNOT_ACCEPT_NEW_REQUESTS);
  }

  // This must be incremented first before checking if the component has started
  // because of a race between transactions entering the component and someone
  // stopping the component.
  active_transactions_count_++;

  if (!started_) {
    active_transactions_count_--;
    return FailureExecutionResult(errors::SC_TRANSACTION_MANAGER_NOT_STARTED);
  }

  function<void()> task = [this, transaction_context]() mutable {
    active_transactions_metric_->Increment(kMetricEventReceivedTransaction);

    // To avoid circular dependency we create a copy. We need to decrement
    // the active transactions.
    auto transaction_engine_context = transaction_context;
    transaction_engine_context.callback =
        [this, transaction_context](
            AsyncContext<TransactionRequest, TransactionResponse>&
                transaction_engine_context) mutable {
          transaction_context.response = transaction_engine_context.response;
          transaction_context.result = transaction_engine_context.result;
          transaction_context.Finish();
          active_transactions_metric_->Increment(
              kMetricEventFinishedTransaction);
          // This should be decremented at the end because of race between
          // transactions leaving the component and someone stopping the
          // component and discarding the component object
          active_transactions_count_--;
        };

    auto execution_result =
        transaction_engine_->Execute(transaction_engine_context);
    if (!execution_result.Successful()) {
      transaction_engine_context.result = execution_result;
      transaction_engine_context.Finish();
    }
  };

  auto execution_result =
      async_executor_->Schedule(task, AsyncPriority::Normal);
  if (!execution_result.Successful()) {
    active_transactions_count_--;
  }
  return execution_result;
}

ExecutionResult TransactionManager::ExecutePhase(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        transaction_phase_context) noexcept {
  if (active_transactions_count_ >= max_concurrent_transactions_) {
    return RetryExecutionResult(
        errors::SC_TRANSACTION_MANAGER_CANNOT_ACCEPT_NEW_REQUESTS);
  }

  // This must be incremented first before checking if the component has started
  // because of a race between transactions entering the component and someone
  // stopping the component.
  active_transactions_count_++;

  if (!started_) {
    active_transactions_count_--;
    return FailureExecutionResult(errors::SC_TRANSACTION_MANAGER_NOT_STARTED);
  }

  function<void()> task = [this, transaction_phase_context]() mutable {
    active_transactions_metric_->Increment(kMetricEventReceivedTransaction);
    // To avoid circular dependency we create a copy. We need to decrement
    // the active transactions.
    auto transaction_engine_context = transaction_phase_context;
    transaction_engine_context.callback =
        [this, transaction_phase_context](
            AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                transaction_engine_context) mutable {
          transaction_phase_context.response =
              transaction_engine_context.response;
          transaction_phase_context.result = transaction_engine_context.result;
          transaction_phase_context.Finish();
          active_transactions_metric_->Increment(
              kMetricEventFinishedTransaction);
          // This should be decremented at the end because of race between
          // transactions leaving the component and someone stopping the
          // component and discarding the component object
          active_transactions_count_--;
        };

    auto execution_result =
        transaction_engine_->ExecutePhase(transaction_engine_context);
    if (!execution_result.Successful()) {
      transaction_engine_context.result = execution_result;
      transaction_engine_context.Finish();
    }
  };

  auto execution_result =
      async_executor_->Schedule(task, AsyncPriority::Normal);
  if (!execution_result.Successful()) {
    active_transactions_count_--;
  }
  return execution_result;
}

ExecutionResult TransactionManager::Checkpoint(
    shared_ptr<list<CheckpointLog>>& checkpoint_logs) noexcept {
  if (started_) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_CANNOT_CREATE_CHECKPOINT_WHEN_STARTED);
  }

  return transaction_engine_->Checkpoint(checkpoint_logs);
}

ExecutionResult TransactionManager::GetTransactionStatus(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context) noexcept {
  active_transactions_count_++;
  if (!started_) {
    active_transactions_count_--;
    return FailureExecutionResult(errors::SC_TRANSACTION_MANAGER_NOT_STARTED);
  }

  auto execution_result =
      transaction_engine_->GetTransactionStatus(get_transaction_status_context);
  active_transactions_count_--;
  return execution_result;
}

ExecutionResult TransactionManager::GetTransactionManagerStatus(
    const GetTransactionManagerStatusRequest& request,
    GetTransactionManagerStatusResponse& response) noexcept {
  active_transactions_count_++;
  // Ensure that we do not report incorrect pending transaction count when the
  // service is initializing.
  if (!started_) {
    active_transactions_count_--;
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_STATUS_CANNOT_BE_OBTAINED);
  }

  // This assumes that caller of GetTransactionManagerStatus always wants to
  // know about pending transaction count, but can be extended to other fields
  // as needed in future.
  response.pending_transactions_count =
      transaction_engine_->GetPendingTransactionCount();
  active_transactions_count_--;
  return SuccessExecutionResult();
}
}  // namespace google::scp::core
