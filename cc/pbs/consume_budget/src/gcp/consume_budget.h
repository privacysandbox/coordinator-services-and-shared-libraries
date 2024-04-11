// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef CC_PBS_CONSUME_BUDGET_SRC_GCP_CONSUME_BUDGET_H_
#define CC_PBS_CONSUME_BUDGET_SRC_GCP_CONSUME_BUDGET_H_

#include <memory>
#include <string>

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/spanner/client.h"

namespace google::scp::pbs {

// A helper class to consume privacy budgets for a given list of privacy budget
// keys by writing to GCP Spanner.
class BudgetConsumptionHelper : public BudgetConsumptionHelperInterface {
 public:
  BudgetConsumptionHelper(
      google::scp::core::ConfigProviderInterface* config_provider,
      google::scp::core::AsyncExecutorInterface* async_executor,
      google::scp::core::AsyncExecutorInterface* io_async_executor,
      std::shared_ptr<cloud::spanner::Connection> spanner_connection);

  google::scp::core::ExecutionResult Init() noexcept override;

  google::scp::core::ExecutionResult Run() noexcept override;

  google::scp::core::ExecutionResult Stop() noexcept override;

  // Consumes privacy budgets for the given list of privacy budget keys in
  // consume_budget_context.
  google::scp::core::ExecutionResult ConsumeBudgets(
      google::scp::core::AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>
          consume_budgets_context) override;

  static google::scp::core::ExecutionResultOr<
      std::shared_ptr<cloud::spanner::Connection>>
  MakeSpannerConnectionForProd(
      google::scp::core::ConfigProviderInterface& config_provider);

 private:
  void ConsumeBudgetsSyncAndFinishContext(
      google::scp::core::AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>
          consume_budgets_context);

  google::scp::core::ExecutionResult ConsumeBudgetsSync(
      const google::scp::core::AsyncContext<ConsumeBudgetsRequest,
                                            ConsumeBudgetsResponse>&
          consume_budgets_context);

  google::scp::core::ConfigProviderInterface* config_provider_;
  google::scp::core::AsyncExecutorInterface* async_executor_;
  google::scp::core::AsyncExecutorInterface* io_async_executor_;
  std::shared_ptr<cloud::spanner::Connection> spanner_connection_;
  std::string table_name_;
};

}  // namespace google::scp::pbs

#endif  // CC_PBS_CONSUME_BUDGET_SRC_GCP_CONSUME_BUDGET_H_
