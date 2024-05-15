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

#include <vector>

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/service_interface.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::scp::pbs {

struct ConsumeBudgetsRequest {
  std::vector<ConsumeBudgetMetadata> budgets;
};

struct ConsumeBudgetsResponse {
  std::vector<size_t> budget_exhausted_indices;
};

// A helper class to consume a given list of budgets.
class BudgetConsumptionHelperInterface
    : public google::scp::core::ServiceInterface {
 public:
  virtual ~BudgetConsumptionHelperInterface() = default;

  virtual google::scp::core::ExecutionResult ConsumeBudgets(
      google::scp::core::AsyncContext<ConsumeBudgetsRequest,
                                      ConsumeBudgetsResponse>
          consume_budgets_context) = 0;
};
}  // namespace google::scp::pbs

#endif  // CC_PBS_INTERFACE_CONSUME_BUDGET_INTERFACE_H_
