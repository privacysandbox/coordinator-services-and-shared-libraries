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

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/synchronization/mutex.h"
#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/partition_types.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "core/telemetry/src/metric/metric_router.h"
#include "core/transaction_manager/interface/transaction_engine_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

namespace google::scp::core {

/*! @copydoc TransactionManagerInterface
 */
class TransactionManager : public TransactionManagerInterface {
 public:
  TransactionManager() = delete;
  TransactionManager(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<TransactionCommandSerializerInterface>&
          transaction_command_serializer,
      std::shared_ptr<JournalServiceInterface>& journal_service,
      std::shared_ptr<RemoteTransactionManagerInterface>&
          remote_transaction_manager,
      size_t max_concurrent_transactions,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      std::shared_ptr<MetricRouter> metric_router,
      std::shared_ptr<ConfigProviderInterface> config_provider,
      const PartitionId& partition_id = kGlobalPartitionId);

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult Execute(AsyncContext<TransactionRequest, TransactionResponse>&
                              transaction_context) noexcept override;

  ExecutionResult ExecutePhase(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_phase_context) noexcept override;

  ExecutionResult Checkpoint(std::shared_ptr<std::list<CheckpointLog>>&
                                 checkpoint_logs) noexcept override;

  ExecutionResult GetTransactionStatus(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override;

  ExecutionResult GetTransactionManagerStatus(
      const GetTransactionManagerStatusRequest& request,
      GetTransactionManagerStatusResponse& response) noexcept override;

 protected:
  // Callback to be used with an OTel ObservableInstrument.
  static void ObserveActiveTransactionsCallback(
      opentelemetry::metrics::ObserverResult observer_result,
      absl::Nonnull<TransactionManager*> self_ptr);

  /**
   * @brief Registers a Aggregate Metric object
   *
   * @param metrics_instance The pointer of the aggregate metric object.
   * @param name The event name of the aggregate metric used to build metric
   * name.
   * @return ExecutionResult
   */
  virtual ExecutionResult RegisterAggregateMetric(
      std::shared_ptr<cpio::AggregateMetricInterface>& metrics_instance,
      const std::string& name) noexcept;

  TransactionManager(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<transaction_manager::TransactionEngineInterface>
          transaction_engine,
      size_t max_concurrent_transactions,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      std::shared_ptr<MetricRouter> metric_router,
      std::shared_ptr<ConfigProviderInterface> config_provider,
      const PartitionId& partition_id = kGlobalPartitionId)
      : max_concurrent_transactions_(max_concurrent_transactions),
        async_executor_(async_executor),
        transaction_engine_(transaction_engine),
        active_transactions_count_(0),
        max_transactions_since_observed_(0),
        started_(false),
        metric_client_(metric_client),
        metric_router_(metric_router),
        config_provider_(config_provider),
        partition_id_(partition_id),
        activity_id_(partition_id),  // Use partition ID as the activity ID for
                                     // the lifetime of this object
        aggregated_metric_interval_ms_(kDefaultAggregatedMetricIntervalMs) {}

  // Max concurrent transactions count.
  const size_t max_concurrent_transactions_;

  // Instance of the async executor.
  std::shared_ptr<AsyncExecutorInterface> async_executor_;

  // Instance of the transaction engine.
  std::shared_ptr<transaction_manager::TransactionEngineInterface>
      transaction_engine_;

  /**
   * @brief Number of active transactions. This needs to be atomic to be changed
   * concurrently.
   */
  std::atomic<size_t> active_transactions_count_;

  /**
   * @brief Maximum number of concurrent active transactions since metric is
   * last observed. Protected by a mutex when it is read/modified together with
   * active_transactions_count_.
   */
  std::atomic<size_t> max_transactions_since_observed_;

  /**
   * @brief Mutex to ensure consistent concurrent reads/writes to transaction
   * counts.
   */
  mutable absl::Mutex transaction_counts_mutex_;

  // Indicates whether the component has started.
  std::atomic<bool> started_;

  // Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  // Keep a MetricRouter member in order to access MetricRouter-owned OTel
  // Instruments.
  //
  // When null, this instance of PBS component does not produce OTel metrics.
  absl::Nullable<std::shared_ptr<MetricRouter>> metric_router_;

  // The OpenTelemetry Meter used for creating and managing metrics.
  std::shared_ptr<opentelemetry::metrics::Meter> meter_;

  // The OpenTelemetry Instrument for the number of active transactions.
  //
  // Reports the maximum number of concurrent active transactions since
  // metric is last observed, because the real-time number of active
  // transactions fluctuates too quickly to be useful.
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      active_transactions_instrument_;

  // The OpenTelemetry Instrument for the number of received transactions.
  std::shared_ptr<opentelemetry::metrics::Counter<uint64_t>>
      received_transactions_instrument_;

  // The OpenTelemetry Instrument for the number of finished transactions.
  std::shared_ptr<opentelemetry::metrics::Counter<uint64_t>>
      finished_transactions_instrument_;

  // The AggregateMetric instance for number of active transactions.
  std::shared_ptr<cpio::AggregateMetricInterface> active_transactions_metric_;

  // Configurations for Transaction Manager are obtained from this.
  std::shared_ptr<ConfigProviderInterface> config_provider_;

  // Id of the encapsulating partition (if any). Defaults to a global
  // partition.
  core::common::Uuid partition_id_;

  // Activity Id of the background activities
  core::common::Uuid activity_id_;

  // The time interval for metrics aggregation.
  TimeDuration aggregated_metric_interval_ms_;

 private:
  // Initialize MetricClient.
  //
  core::ExecutionResult InitMetricClientInterface();
};

}  // namespace google::scp::core
