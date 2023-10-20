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
  MockTransactionManager(
      std::shared_ptr<AsyncExecutorInterface> async_executor,
      std::shared_ptr<transaction_manager::TransactionEngineInterface>
          transaction_engine,
      size_t max_concurrent_transactions,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client)
      : TransactionManager(
            async_executor, transaction_engine, max_concurrent_transactions,
            metric_client,
            std::make_shared<config_provider::mock::MockConfigProvider>()) {}

  virtual ExecutionResult RegisterAggregateMetric(
      std::shared_ptr<cpio::AggregateMetricInterface>& metrics_instance,
      const std::string& name) noexcept {
    metrics_instance = std::make_shared<cpio::MockAggregateMetric>();
    return SuccessExecutionResult();
  }

  std::atomic<size_t>& GetActiveTransactionsCount() {
    return core::TransactionManager::active_transactions_count_;
  }
};
}  // namespace google::scp::core::transaction_manager::mock
