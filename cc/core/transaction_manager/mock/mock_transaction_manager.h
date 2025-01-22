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

#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/transaction_manager/interface/transaction_engine_interface.h"
#include "cc/core/transaction_manager/src/transaction_manager.h"

namespace google::scp::core::transaction_manager::mock {
class MockTransactionManager : public core::TransactionManager {
 public:
  MockTransactionManager(
      std::shared_ptr<AsyncExecutorInterface> async_executor,
      std::shared_ptr<transaction_manager::TransactionEngineInterface>
          transaction_engine,
      size_t max_concurrent_transactions)
      : TransactionManager(
            async_executor, transaction_engine, max_concurrent_transactions,
            /*metric_router=*/nullptr,
            std::make_shared<config_provider::mock::MockConfigProvider>()) {}

  std::atomic<size_t>& GetActiveTransactionsCount() {
    return core::TransactionManager::active_transactions_count_;
  }
};
}  // namespace google::scp::core::transaction_manager::mock
