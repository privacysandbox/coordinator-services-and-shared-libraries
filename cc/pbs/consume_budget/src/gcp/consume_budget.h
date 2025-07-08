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
#include "google/cloud/spanner/connection.h"

namespace privacy_sandbox::pbs {

// A helper class to consume privacy budgets for a given list of privacy budget
// keys by writing to GCP Spanner.
class BudgetConsumptionHelper : public BudgetConsumptionHelperInterface {
 public:
  BudgetConsumptionHelper(
      pbs_common::ConfigProviderInterface* config_provider,
      pbs_common::AsyncExecutorInterface* async_executor,
      pbs_common::AsyncExecutorInterface* io_async_executor,
      std::shared_ptr<google::cloud::spanner::Connection> spanner_connection);

  pbs_common::ExecutionResult Init() noexcept override;

  pbs_common::ExecutionResult Run() noexcept override;

  pbs_common::ExecutionResult Stop() noexcept override;

  // Consumes privacy budgets for the given list of privacy budget keys in
  // consume_budget_context.
  pbs_common::ExecutionResult ConsumeBudgets(
      pbs_common::AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
          consume_budgets_context) override;

  static pbs_common::ExecutionResultOr<
      std::shared_ptr<google::cloud::spanner::Connection>>
  MakeSpannerConnectionForProd(
      pbs_common::ConfigProviderInterface& config_provider);

 private:
  void ConsumeBudgetsSyncAndFinishContext(
      pbs_common::AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
          consume_budgets_context);

  pbs_common::ExecutionResult ConsumeBudgetsSyncWithBudgetConsumer(
      const pbs_common::AsyncContext<ConsumeBudgetsRequest,
                                     ConsumeBudgetsResponse>&
          consume_budgets_context);

  pbs_common::ConfigProviderInterface* config_provider_;
  pbs_common::AsyncExecutorInterface* async_executor_;
  pbs_common::AsyncExecutorInterface* io_async_executor_;
  std::shared_ptr<google::cloud::spanner::Connection> spanner_connection_;
  std::string table_name_;
};

}  // namespace privacy_sandbox::pbs

#endif  // CC_PBS_CONSUME_BUDGET_SRC_GCP_CONSUME_BUDGET_H_
