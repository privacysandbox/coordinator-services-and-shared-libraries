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

#ifndef CC_PBS_INTERFACE_CONSUME_BUDGET_INTERFACE_H_
#define CC_PBS_INTERFACE_CONSUME_BUDGET_INTERFACE_H_

#include <memory>
#include <vector>

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/service_interface.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs {

struct ConsumeBudgetsRequest {
  std::vector<ConsumeBudgetMetadata> budgets;
  std::unique_ptr<BudgetConsumer> budget_consumer;
};

struct ConsumeBudgetsResponse {
  std::vector<size_t> budget_exhausted_indices;
};

// A helper class to consume a given list of budgets.
class BudgetConsumptionHelperInterface : public pbs_common::ServiceInterface {
 public:
  virtual ~BudgetConsumptionHelperInterface() = default;

  virtual pbs_common::ExecutionResult ConsumeBudgets(
      pbs_common::AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
          consume_budgets_context) = 0;
};
}  // namespace privacy_sandbox::pbs

#endif  // CC_PBS_INTERFACE_CONSUME_BUDGET_INTERFACE_H_
