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

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "core/transaction_manager/interface/transaction_engine_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/aggregate_metric_interface.h"

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
      const std::shared_ptr<
          cpio::client_providers::MetricClientProviderInterface>& metric_client,
      std::shared_ptr<ConfigProviderInterface> config_provider);

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

  ExecutionResult GetStatus(
      const GetTransactionManagerStatusRequest& request,
      GetTransactionManagerStatusResponse& response) noexcept override;

 protected:
  /**
   * @brief Registers a Aggregate Metric object
   *
   * @param metrics_instance The pointer of the aggregate metric object.
   * @param name The event name of the aggregate metric used to build metric
   * name.
   * @return ExecutionResult
   */
  virtual ExecutionResult RegisterAggregateMetric(
      std::shared_ptr<cpio::client_providers::AggregateMetricInterface>&
          metrics_instance,
      const std::string& name) noexcept;

  TransactionManager(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<transaction_manager::TransactionEngineInterface>
          transaction_engine,
      size_t max_concurrent_transactions,
      const std::shared_ptr<
          cpio::client_providers::MetricClientProviderInterface>& metric_client,
      std::shared_ptr<ConfigProviderInterface> config_provider)
      : max_concurrent_transactions_(max_concurrent_transactions),
        async_executor_(async_executor),
        transaction_engine_(transaction_engine),
        active_transactions_count_(0),
        started_(false),
        metric_client_(metric_client),
        config_provider_(config_provider) {}

  /// Max concurrent transactions count.
  const size_t max_concurrent_transactions_;

  /// Instance of the async executor.
  std::shared_ptr<AsyncExecutorInterface> async_executor_;

  /// Instance of the transaction engine.
  std::shared_ptr<transaction_manager::TransactionEngineInterface>
      transaction_engine_;

  /**
   * @brief Number of active transactions. This needs to be atomic to be changed
   * concurrently.
   */
  std::atomic<size_t> active_transactions_count_;

  /// Indicates whether the component has started.
  std::atomic<bool> started_;

  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::client_providers::MetricClientProviderInterface>
      metric_client_;

  /// The AggregateMetric instance for number of active transactions.
  std::shared_ptr<cpio::client_providers::AggregateMetricInterface>
      active_transactions_metric_;

  /// Configurations for Transaction Manager are obtained from this.
  std::shared_ptr<ConfigProviderInterface> config_provider_;
};
}  // namespace google::scp::core
