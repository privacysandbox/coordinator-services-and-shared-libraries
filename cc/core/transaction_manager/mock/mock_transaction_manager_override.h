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
#include <functional>
#include <memory>
#include <string>

#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_executor_interface.h"
#include "core/transaction_manager/interface/transaction_engine_interface.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

namespace google::scp::core::transaction_manager::mock {
class MockTransactionManager : public core::TransactionManager {
 public:
  MockTransactionManager() = delete;

  MockTransactionManager(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<TransactionCommandSerializerInterface>&
          transaction_command_serializer,
      std::shared_ptr<JournalServiceInterface>& journal_service,
      std::shared_ptr<RemoteTransactionManagerInterface>&
          remote_transaction_manager,
      size_t max_concurrent_transactions,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client)
      : TransactionManager(
            async_executor, transaction_command_serializer, journal_service,
            remote_transaction_manager, max_concurrent_transactions,
            metric_client,
            std::make_shared<config_provider::mock::MockConfigProvider>()) {}

  std::function<ExecutionResult(
      AsyncContext<TransactionRequest, TransactionResponse>&)>
      execute_mock;

  std::function<ExecutionResult(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&)>
      execute_phase_mock;

  std::function<ExecutionResult(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&)>
      get_transaction_status_mock;

  std::function<ExecutionResult(
      const GetTransactionManagerStatusRequest& request,
      GetTransactionManagerStatusResponse& response)>
      get_status_mock;

  ExecutionResult RegisterAggregateMetric(
      std::shared_ptr<cpio::AggregateMetricInterface>& metrics_instance,
      const std::string& name) noexcept {
    metrics_instance = std::make_shared<cpio::MockAggregateMetric>();
    return SuccessExecutionResult();
  }

  ExecutionResult Execute(AsyncContext<TransactionRequest, TransactionResponse>&
                              transaction_context) noexcept override {
    if (execute_mock) {
      return execute_mock(transaction_context);
    }

    return TransactionManager::Execute(transaction_context);
  }

  ExecutionResult ExecutePhase(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_context) noexcept override {
    if (execute_phase_mock) {
      return execute_phase_mock(transaction_context);
    }

    return TransactionManager::ExecutePhase(transaction_context);
  }

  ExecutionResult GetTransactionStatus(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override {
    if (get_transaction_status_mock) {
      return get_transaction_status_mock(get_transaction_status_context);
    }

    return TransactionManager::GetTransactionStatus(
        get_transaction_status_context);
  }

  ExecutionResult GetTransactionManagerStatus(
      const GetTransactionManagerStatusRequest& request,
      GetTransactionManagerStatusResponse& response) noexcept override {
    if (get_status_mock) {
      return get_status_mock(request, response);
    }

    return TransactionManager::GetTransactionManagerStatus(request, response);
  }
};
}  // namespace google::scp::core::transaction_manager::mock
