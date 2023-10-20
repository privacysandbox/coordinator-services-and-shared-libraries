// Copyright 2022 Google LLC
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

#include "pbs/budget_key_transaction_protocols/src/transaction_protocol_helpers.h"

#include <gtest/gtest.h>

#include "pbs/interface/budget_key_interface.h"

using std::make_shared;
using std::shared_ptr;
using std::vector;

namespace google::scp::pbs::test {

TEST(TransactionProtocolHelpersTest, AreBudgetsInIncreasingOrder) {
  vector<BudgetConsumptionRequestInfo> budgets;
  EXPECT_EQ(TransactionProtocolHelpers::AreBudgetsInIncreasingOrder(budgets),
            true);

  budgets.emplace_back();
  budgets.back().time_bucket = 2;
  EXPECT_EQ(TransactionProtocolHelpers::AreBudgetsInIncreasingOrder(budgets),
            true);

  budgets.emplace_back();
  budgets.back().time_bucket = 200;
  EXPECT_EQ(TransactionProtocolHelpers::AreBudgetsInIncreasingOrder(budgets),
            true);

  budgets.emplace_back();
  budgets.back().time_bucket = 1;
  EXPECT_EQ(TransactionProtocolHelpers::AreBudgetsInIncreasingOrder(budgets),
            false);

  budgets.emplace_back();
  budgets.back().time_bucket = 5;
  EXPECT_EQ(TransactionProtocolHelpers::AreBudgetsInIncreasingOrder(budgets),
            false);
}

TEST(TransactionProtocolHelpersTest, ReleaseAcquiredLocksOnTimeframes) {
  vector<shared_ptr<BudgetKeyTimeframe>> timeframes;
  core::common::Uuid transaction_id = {2, 5};
  core::common::Uuid transaction_id_other = {2, 6};

  timeframes.push_back(make_shared<BudgetKeyTimeframe>(0));
  timeframes.back()->active_transaction_id = transaction_id;

  TransactionProtocolHelpers::ReleaseAcquiredLocksOnTimeframes(transaction_id,
                                                               timeframes);
  EXPECT_EQ(timeframes.back()->active_transaction_id.load().high,
            core::common::kZeroUuid.high);
  EXPECT_EQ(timeframes.back()->active_transaction_id.load().low,
            core::common::kZeroUuid.low);

  timeframes.push_back(make_shared<BudgetKeyTimeframe>(1));
  timeframes.back()->active_transaction_id = transaction_id;

  timeframes.push_back(make_shared<BudgetKeyTimeframe>(2));
  timeframes.back()->active_transaction_id = transaction_id;

  timeframes.push_back(make_shared<BudgetKeyTimeframe>(3));
  timeframes.back()->active_transaction_id = transaction_id_other;

  TransactionProtocolHelpers::ReleaseAcquiredLocksOnTimeframes(transaction_id,
                                                               timeframes);

  EXPECT_EQ(timeframes[0]->active_transaction_id.load().high,
            core::common::kZeroUuid.high);
  EXPECT_EQ(timeframes[0]->active_transaction_id.load().low,
            core::common::kZeroUuid.low);

  EXPECT_EQ(timeframes[1]->active_transaction_id.load().high,
            core::common::kZeroUuid.high);
  EXPECT_EQ(timeframes[1]->active_transaction_id.load().low,
            core::common::kZeroUuid.low);

  EXPECT_EQ(timeframes[2]->active_transaction_id.load().high,
            core::common::kZeroUuid.high);
  EXPECT_EQ(timeframes[2]->active_transaction_id.load().low,
            core::common::kZeroUuid.low);

  EXPECT_EQ(timeframes[3]->active_transaction_id.load().high,
            transaction_id_other.high);
  EXPECT_EQ(timeframes[3]->active_transaction_id.load().low,
            transaction_id_other.low);
}
}  // namespace google::scp::pbs::test
