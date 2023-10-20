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

#include <memory>
#include <unordered_map>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"

namespace google::scp::pbs {

class TransactionProtocolHelpers {
 public:
  /**
   * @brief Release locks on the time frames (if any) acquired by the specified
   * transaction
   *
   * @param timeframes timeframes on which locks need to be release if already
   * acquired
   */
  static void ReleaseAcquiredLocksOnTimeframes(
      const core::common::Uuid& transaction_id,
      std::vector<std::shared_ptr<BudgetKeyTimeframe>>& timeframes) {
    for (unsigned int i = 0; i < timeframes.size(); i++) {
      if (timeframes[i]->active_transaction_id.load() == transaction_id) {
        // Lock is held by this transaction, release it by setting '0'
        timeframes[i]->active_transaction_id = core::common::kZeroUuid;
      }
    }
  }

  /**
   * @brief Returns true if time buckets are in increasing order, else returns
   * false.
   * This is useful is ensuring that requests arrive with budgets in the same
   * order every time, which avoids any potential deadlocks.
   *
   * @param budgets_to_consume
   * @return true
   * @return false
   */
  static bool AreBudgetsInIncreasingOrder(
      const std::vector<BudgetConsumptionRequestInfo>& budgets) {
    TimeBucket prev_time_bucket = 0;
    for (const auto& budget : budgets) {
      if (budget.time_bucket < prev_time_bucket) {
        return false;
      }
      prev_time_bucket = budget.time_bucket;
    }
    return true;
  }
};
}  // namespace google::scp::pbs
