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

#include "pbs/budget_key_transaction_protocols/src/batch_consume_budget_transaction_protocol.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/type_def.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/budget_key_timeframe_manager/mock/mock_budget_key_timeframe_manager.h"
#include "pbs/budget_key_transaction_protocols/mock/mock_consume_budget_transaction_protocol_with_overrides.h"
#include "pbs/budget_key_transaction_protocols/src/error_codes.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::Uuid;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::pbs::BudgetKeyTimeframe;
using google::scp::pbs::budget_key::mock::
    MockConsumeBudgetTransactionProtocolWithOverrides;
using google::scp::pbs::buget_key_timeframe_manager::mock::
    MockBudgetKeyTimeframeManager;
using std::atomic;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::swap;
using std::vector;
using std::chrono::hours;
using std::chrono::nanoseconds;
using testing::ElementsAre;

namespace google::scp::pbs::test {

TEST(BatchConsumeBudgetTransactionProtocolTest, PrepareInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  PrepareBatchConsumeBudgetRequest prepare_batch_consume_budget_request;
  prepare_batch_consume_budget_request.budget_consumptions = {};

  AsyncContext<PrepareBatchConsumeBudgetRequest,
               PrepareBatchConsumeBudgetResponse>
      prepare_batch_consume_budget_context(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          [&](auto& prepare_batch_consume_budget_context) {
            // Will not be called
          });
  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));

  // Transaction ID is valid
  prepare_batch_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(
      transaction_protocol->Prepare(prepare_batch_consume_budget_context),
      FailureExecutionResult(
          core::errors::
              SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_LESS_BUDGETS_TO_CONSUME));

  // Budgets are valid
  prepare_batch_consume_budget_context.request->budget_consumptions = {
      {1, 1}, {100000000, 1}};
  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(BatchConsumeBudgetTransactionProtocolTest, PrepareInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareBatchConsumeBudgetRequest prepare_batch_consume_budget_request;
  prepare_batch_consume_budget_request.transaction_id = {0, 1};
  prepare_batch_consume_budget_request.budget_consumptions = {{0, 1},
                                                              {100000, 1}};

  AsyncContext<PrepareBatchConsumeBudgetRequest,
               PrepareBatchConsumeBudgetResponse>
      prepare_batch_consume_budget_context(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          [&](auto& prepare_batch_consume_budget_context) {
            EXPECT_EQ(prepare_batch_consume_budget_context.result,
                      FailureExecutionResult(1234));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest,
     PrepareActiveTransactionInProgressOnOneOfTimeframes) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({1, 2});
  budget_key_timeframe2->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareBatchConsumeBudgetRequest prepare_batch_consume_budget_request;
  prepare_batch_consume_budget_request.transaction_id = {0, 1};
  prepare_batch_consume_budget_request.budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 1}};

  AsyncContext<PrepareBatchConsumeBudgetRequest,
               PrepareBatchConsumeBudgetResponse>
      prepare_batch_consume_budget_context(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          [&](auto& prepare_batch_consume_budget_context) {
            EXPECT_EQ(
                prepare_batch_consume_budget_context.result,
                RetryExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe1->time_bucket_index, timeframe_bucket1);
  EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe1->active_token_count, 0);

  EXPECT_EQ(budget_key_timeframe2->time_bucket_index, timeframe_bucket2);
  EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({1, 2}));
  EXPECT_EQ(budget_key_timeframe2->active_token_count, 0);

  // Swap the transaction ids on the budget frames.
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({1, 2});
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  condition = false;

  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe1->time_bucket_index, timeframe_bucket1);
  EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({1, 2}));
  EXPECT_EQ(budget_key_timeframe1->active_token_count, 0);

  EXPECT_EQ(budget_key_timeframe2->time_bucket_index, timeframe_bucket2);
  EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe2->active_token_count, 0);
}

TEST(BatchConsumeBudgetTransactionProtocolTest,
     PrepareInsufficientTokensOnOneOfTimeframes) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  // Set up timeframes to be loaded
  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(8)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe3->token_count = 5;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareBatchConsumeBudgetRequest prepare_batch_consume_budget_request;
  prepare_batch_consume_budget_request.transaction_id = {0, 1};
  prepare_batch_consume_budget_request.budget_consumptions = {
      {timeframe_bucket1, 10}, {timeframe_bucket2, 1}, {timeframe_bucket3, 8}};

  AsyncContext<PrepareBatchConsumeBudgetRequest,
               PrepareBatchConsumeBudgetResponse>
      prepare_batch_consume_budget_context(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_EQ(
                context.result,
                FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
            EXPECT_THAT(context.response->failed_budget_consumption_indices,
                        ElementsAre(0, 2));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });

  EXPECT_EQ(budget_key_timeframe1->time_bucket_index, timeframe_bucket1);
  EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe1->active_token_count, 0);

  EXPECT_EQ(budget_key_timeframe2->time_bucket_index, timeframe_bucket2);
  EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe2->active_token_count, 0);

  EXPECT_EQ(budget_key_timeframe3->time_bucket_index, timeframe_bucket3);
  EXPECT_EQ(budget_key_timeframe3->token_count.load(), 5);
  EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe3->active_token_count, 0);

  prepare_batch_consume_budget_context =
      AsyncContext<PrepareBatchConsumeBudgetRequest,
                   PrepareBatchConsumeBudgetResponse>(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_EQ(
                context.result,
                FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
            EXPECT_THAT(context.response->failed_budget_consumption_indices,
                        ElementsAre(1, 2));
            condition = true;
          });

  // Swap the budgets on the budget frames.
  prepare_batch_consume_budget_context.request->budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 10}, {timeframe_bucket3, 8}};
  condition = false;

  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe1->time_bucket_index, timeframe_bucket1);
  EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe1->active_token_count, 0);

  EXPECT_EQ(budget_key_timeframe2->time_bucket_index, timeframe_bucket2);
  EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe2->active_token_count, 0);

  EXPECT_EQ(budget_key_timeframe3->time_bucket_index, timeframe_bucket3);
  EXPECT_EQ(budget_key_timeframe3->token_count.load(), 5);
  EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe3->active_token_count, 0);

  prepare_batch_consume_budget_context =
      AsyncContext<PrepareBatchConsumeBudgetRequest,
                   PrepareBatchConsumeBudgetResponse>(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_EQ(
                context.result,
                FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
            EXPECT_EQ(
                context.response->failed_budget_consumption_indices.size(), 0);
            condition = true;
          });

  // Consuming sufficient tokens would pass
  prepare_batch_consume_budget_context.request->budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 1}, {timeframe_bucket3, 1}};
  prepare_batch_consume_budget_context.callback = [&](auto& context) {
    EXPECT_SUCCESS(context.result);
    condition = true;
  };
  condition = false;

  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest, PrepareSufficientTokens) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareBatchConsumeBudgetRequest prepare_batch_consume_budget_request;
  prepare_batch_consume_budget_request.transaction_id = {0, 1};
  prepare_batch_consume_budget_request.budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 1}};

  AsyncContext<PrepareBatchConsumeBudgetRequest,
               PrepareBatchConsumeBudgetResponse>
      prepare_batch_consume_budget_context(
          make_shared<PrepareBatchConsumeBudgetRequest>(
              move(prepare_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_SUCCESS(context.result);
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest, CommitInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
  commit_batch_consume_budget_request.budget_consumptions = {};

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_batch_consume_budget_context(
          make_shared<CommitBatchConsumeBudgetRequest>(
              move(commit_batch_consume_budget_request)),
          [&](auto& commit_batch_consume_budget_context) {
            // Will not be called
          });
  EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));

  // Transaction ID is valid
  commit_batch_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(
      transaction_protocol->Commit(commit_batch_consume_budget_context),
      FailureExecutionResult(
          core::errors::
              SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_LESS_BUDGETS_TO_CONSUME));

  // Budgets are valid but in wrong order
  commit_batch_consume_budget_context.request->budget_consumptions = {
      {100000000, 1}, {1, 1}};
  EXPECT_EQ(
      transaction_protocol->Commit(commit_batch_consume_budget_context),
      FailureExecutionResult(
          core::errors::SC_PBS_BUDGET_KEY_BATCH_REQUEST_HAS_INVALID_ORDER));

  // Budgets are valid
  commit_batch_consume_budget_context.request->budget_consumptions = {
      {1, 1}, {100000000, 1}};
  EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(BatchConsumeBudgetTransactionProtocolTest, CommitInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
  commit_batch_consume_budget_request.transaction_id = {0, 1};
  commit_batch_consume_budget_request.budget_consumptions = {{0, 1},
                                                             {100000, 1}};

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_batch_consume_budget_context(
          make_shared<CommitBatchConsumeBudgetRequest>(
              move(commit_batch_consume_budget_request)),
          [&](auto& commit_batch_consume_budget_context) {
            EXPECT_EQ(commit_batch_consume_budget_context.result,
                      FailureExecutionResult(1234));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest, CommitRetry) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> request_finished = false;
  atomic<bool> update_invoked = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({1, 2});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->update_function =
      [&](auto& update_budget_key_timeframe_context) {
        update_invoked = true;
        return SuccessExecutionResult();
      };

  CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
  commit_batch_consume_budget_request.transaction_id =
      core::common::Uuid({1, 2});
  commit_batch_consume_budget_request.budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 1}};

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_batch_consume_budget_context(
          make_shared<CommitBatchConsumeBudgetRequest>(
              move(commit_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_SUCCESS(context.result);
            request_finished = true;
          });

  EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return request_finished.load(); });
  EXPECT_EQ(update_invoked.load(), false);
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({1, 2}));
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
}

TEST(BatchConsumeBudgetTransactionProtocolTest,
     CommitCannotAcquireLockReleasesAllAcquiredLocks) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> request_finished = false;
  atomic<bool> update_invoked = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe3->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->update_function =
      [&](auto& update_budget_key_timeframe_context) {
        update_invoked = true;
        return SuccessExecutionResult();
      };

  // First timeframe is already locked by some other txn.
  {
    budget_key_timeframe1->active_transaction_id = core::common::Uuid({100, 0});
    budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});

    CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
    commit_batch_consume_budget_request.transaction_id =
        core::common::Uuid({1, 2});
    commit_batch_consume_budget_request.budget_consumptions = {
        {timeframe_bucket1, 1}, {timeframe_bucket2, 1}, {timeframe_bucket3, 1}};

    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>
        commit_batch_consume_budget_context(
            make_shared<CommitBatchConsumeBudgetRequest>(
                move(commit_batch_consume_budget_request)),
            [&](auto& context) {
              EXPECT_EQ(
                  context.result,
                  RetryExecutionResult(
                      core::errors::
                          SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS));
              request_finished = true;
            });

    EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
              SuccessExecutionResult());
    WaitUntil([&]() { return request_finished.load(); });
    EXPECT_EQ(update_invoked.load(), false);
    EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
              core::common::Uuid({100, 0}));
    EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
              core::common::Uuid({0, 0}));
    EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
              core::common::Uuid({0, 0}));

    // Timeframes are not modified
    EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe3->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe1->active_token_count.load(), 0);
    EXPECT_EQ(budget_key_timeframe2->active_token_count.load(), 0);
    EXPECT_EQ(budget_key_timeframe3->active_token_count.load(), 0);
  }

  // Second timeframe is already locked by some other txn.
  {
    budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe2->active_transaction_id = core::common::Uuid({100, 0});
    budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});

    CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
    commit_batch_consume_budget_request.transaction_id =
        core::common::Uuid({1, 2});
    commit_batch_consume_budget_request.budget_consumptions = {
        {timeframe_bucket1, 1}, {timeframe_bucket2, 1}, {timeframe_bucket3, 1}};

    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>
        commit_batch_consume_budget_context(
            make_shared<CommitBatchConsumeBudgetRequest>(
                move(commit_batch_consume_budget_request)),
            [&](auto& context) {
              EXPECT_EQ(
                  context.result,
                  RetryExecutionResult(
                      core::errors::
                          SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS));
              request_finished = true;
            });

    EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
              SuccessExecutionResult());
    WaitUntil([&]() { return request_finished.load(); });
    EXPECT_EQ(update_invoked.load(), false);
    EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
              core::common::Uuid({0, 0}));
    EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
              core::common::Uuid({100, 0}));
    EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
              core::common::Uuid({0, 0}));

    // Timeframes are not modified
    EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe3->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe1->active_token_count.load(), 0);
    EXPECT_EQ(budget_key_timeframe2->active_token_count.load(), 0);
    EXPECT_EQ(budget_key_timeframe3->active_token_count.load(), 0);
  }

  // Third timeframe is already locked by some other txn.
  {
    budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe3->active_transaction_id = core::common::Uuid({100, 0});

    CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
    commit_batch_consume_budget_request.transaction_id =
        core::common::Uuid({1, 2});
    commit_batch_consume_budget_request.budget_consumptions = {
        {timeframe_bucket1, 1}, {timeframe_bucket2, 1}, {timeframe_bucket3, 1}};

    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>
        commit_batch_consume_budget_context(
            make_shared<CommitBatchConsumeBudgetRequest>(
                move(commit_batch_consume_budget_request)),
            [&](auto& context) {
              EXPECT_EQ(
                  context.result,
                  RetryExecutionResult(
                      core::errors::
                          SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS));
              request_finished = true;
            });

    EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
              SuccessExecutionResult());
    WaitUntil([&]() { return request_finished.load(); });
    EXPECT_EQ(update_invoked.load(), false);
    EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
              core::common::Uuid({0, 0}));
    EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
              core::common::Uuid({0, 0}));
    EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
              core::common::Uuid({100, 0}));

    // Timeframes are not modified
    EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe3->token_count.load(), 2);
    EXPECT_EQ(budget_key_timeframe1->active_token_count.load(), 0);
    EXPECT_EQ(budget_key_timeframe2->active_token_count.load(), 0);
    EXPECT_EQ(budget_key_timeframe3->active_token_count.load(), 0);
  }
}

TEST(BatchConsumeBudgetTransactionProtocolTest,
     CommitInsufficientBudgetOnAtleastOneTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> request_finished = false;
  atomic<bool> update_invoked = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe3->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->update_function =
      [&](auto& update_budget_key_timeframe_context) {
        update_invoked = true;
        return SuccessExecutionResult();
      };

  CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
  commit_batch_consume_budget_request.transaction_id =
      core::common::Uuid({1, 2});
  commit_batch_consume_budget_request.budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 1}, {timeframe_bucket3, 3}};

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_batch_consume_budget_context(
          make_shared<CommitBatchConsumeBudgetRequest>(
              move(commit_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_EQ(
                context.result,
                FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
            EXPECT_THAT(context.response->failed_budget_consumption_indices,
                        ElementsAre(2));
            request_finished = true;
          });

  EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return request_finished.load(); });
  EXPECT_EQ(update_invoked.load(), false);

  // Released all locks
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
            core::common::Uuid({0, 0}));

  // Timeframes are not modified
  EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe3->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe1->active_token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe2->active_token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe3->active_token_count.load(), 0);
}

TEST(BatchConsumeBudgetTransactionProtocolTest, CommitBudgetLogUpdateFails) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> request_finished = false;
  atomic<bool> update_invoked = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe3->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->update_function =
      [&](auto& update_budget_key_timeframe_context) {
        update_invoked = true;
        return FailureExecutionResult(1234);
      };

  CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
  commit_batch_consume_budget_request.transaction_id =
      core::common::Uuid({1, 2});
  commit_batch_consume_budget_request.budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 1}, {timeframe_bucket3, 1}};

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_batch_consume_budget_context(
          make_shared<CommitBatchConsumeBudgetRequest>(
              move(commit_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
            request_finished = true;
          });

  EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return request_finished.load(); });
  EXPECT_EQ(update_invoked.load(), true);

  // Released all locks
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
  EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
            core::common::Uuid({0, 0}));

  // Timeframes are not modified
  EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe3->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe1->active_token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe2->active_token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe3->active_token_count.load(), 0);
}

TEST(BatchConsumeBudgetTransactionProtocolTest, CommitSufficientBudget) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> request_finished = false;
  atomic<bool> update_invoked = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe3->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->update_function = [&](auto& context) {
    update_invoked = true;
    // Verify
    EXPECT_EQ(context.request->timeframes_to_update.size(), 3);
    EXPECT_EQ(context.request->timeframes_to_update[0].active_token_count, 1);
    EXPECT_EQ(context.request->timeframes_to_update[0].active_transaction_id,
              core::common::Uuid({1, 2}));
    EXPECT_EQ(context.request->timeframes_to_update[0].reporting_time,
              timeframe_bucket1);
    EXPECT_EQ(context.request->timeframes_to_update[0].token_count, 2);

    EXPECT_EQ(context.request->timeframes_to_update[1].active_token_count, 1);
    EXPECT_EQ(context.request->timeframes_to_update[1].active_transaction_id,
              core::common::Uuid({1, 2}));
    EXPECT_EQ(context.request->timeframes_to_update[1].reporting_time,
              timeframe_bucket2);
    EXPECT_EQ(context.request->timeframes_to_update[1].token_count, 2);

    EXPECT_EQ(context.request->timeframes_to_update[2].active_token_count, 1);
    EXPECT_EQ(context.request->timeframes_to_update[2].active_transaction_id,
              core::common::Uuid({1, 2}));
    EXPECT_EQ(context.request->timeframes_to_update[2].reporting_time,
              timeframe_bucket3);
    EXPECT_EQ(context.request->timeframes_to_update[2].token_count, 2);

    // Update
    budget_key_timeframe1->active_token_count =
        context.request->timeframes_to_update[0].active_token_count;
    budget_key_timeframe2->active_token_count =
        context.request->timeframes_to_update[1].active_token_count;
    budget_key_timeframe3->active_token_count =
        context.request->timeframes_to_update[2].active_token_count;

    context.result = SuccessExecutionResult();
    context.Finish();
    return SuccessExecutionResult();
  };

  CommitBatchConsumeBudgetRequest commit_batch_consume_budget_request;
  commit_batch_consume_budget_request.transaction_id =
      core::common::Uuid({1, 2});
  commit_batch_consume_budget_request.budget_consumptions = {
      {timeframe_bucket1, 1}, {timeframe_bucket2, 1}, {timeframe_bucket3, 1}};

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_batch_consume_budget_context(
          make_shared<CommitBatchConsumeBudgetRequest>(
              move(commit_batch_consume_budget_request)),
          [&](auto& context) {
            EXPECT_SUCCESS(context.result);
            request_finished = true;
          });

  EXPECT_EQ(transaction_protocol->Commit(commit_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return request_finished.load(); });
  EXPECT_EQ(update_invoked.load(), true);

  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({1, 2}));
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({1, 2}));
  EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
            core::common::Uuid({1, 2}));

  EXPECT_EQ(budget_key_timeframe1->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe2->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe3->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe1->active_token_count.load(), 1);
  EXPECT_EQ(budget_key_timeframe2->active_token_count.load(), 1);
  EXPECT_EQ(budget_key_timeframe3->active_token_count.load(), 1);
}

TEST(BatchConsumeBudgetTransactionProtocolTest, NotifyInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  NotifyBatchConsumeBudgetRequest notify_batch_consume_budget_request;
  notify_batch_consume_budget_request.time_buckets = {1, 100000000};

  AsyncContext<NotifyBatchConsumeBudgetRequest,
               NotifyBatchConsumeBudgetResponse>
      notify_batch_consume_budget_context(
          make_shared<NotifyBatchConsumeBudgetRequest>(
              move(notify_batch_consume_budget_request)),
          [&](auto& notify_batch_consume_budget_context) {
            // Will not be called
          });
  EXPECT_EQ(transaction_protocol->Notify(notify_batch_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));

  // Transaction ID is valid
  notify_batch_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(transaction_protocol->Notify(notify_batch_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(BatchConsumeBudgetTransactionProtocolTest, NotifyInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  NotifyBatchConsumeBudgetRequest notify_batch_consume_budget_request;
  notify_batch_consume_budget_request.time_buckets = {1, 100000000};
  notify_batch_consume_budget_request.transaction_id = {1, 1};

  atomic<bool> condition = false;
  AsyncContext<NotifyBatchConsumeBudgetRequest,
               NotifyBatchConsumeBudgetResponse>
      notify_batch_consume_budget_context(
          make_shared<NotifyBatchConsumeBudgetRequest>(
              move(notify_batch_consume_budget_request)),
          [&](auto& notify_batch_consume_budget_context) {
            EXPECT_EQ(notify_batch_consume_budget_context.result,
                      FailureExecutionResult(1234));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Notify(notify_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest, NotifyRetry) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({3, 4});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe3->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  NotifyBatchConsumeBudgetRequest notify_batch_consume_budget_request;
  notify_batch_consume_budget_request.time_buckets = {
      timeframe_bucket1, timeframe_bucket2, timeframe_bucket3};
  notify_batch_consume_budget_request.transaction_id = {1, 1};

  atomic<bool> condition = false;
  AsyncContext<NotifyBatchConsumeBudgetRequest,
               NotifyBatchConsumeBudgetResponse>
      notify_batch_consume_budget_context(
          make_shared<NotifyBatchConsumeBudgetRequest>(
              move(notify_batch_consume_budget_request)),
          [&](auto& notify_batch_consume_budget_context) {
            EXPECT_EQ(notify_batch_consume_budget_context.result,
                      SuccessExecutionResult());
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Notify(notify_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest, Notify) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> update_invoked = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({1, 1});
  budget_key_timeframe1->token_count = 2;
  budget_key_timeframe1->active_token_count = 1;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({1, 1});
  budget_key_timeframe2->token_count = 2;
  budget_key_timeframe2->active_token_count = 1;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({1, 1});
  budget_key_timeframe3->token_count = 2;
  budget_key_timeframe3->active_token_count = 1;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->update_function = [&](auto& context) {
    update_invoked = true;
    // Verify
    EXPECT_EQ(context.request->timeframes_to_update.size(), 3);
    EXPECT_EQ(context.request->timeframes_to_update[0].active_token_count, 0);
    EXPECT_EQ(context.request->timeframes_to_update[0].active_transaction_id,
              core::common::Uuid({0, 0}));
    EXPECT_EQ(context.request->timeframes_to_update[0].reporting_time,
              timeframe_bucket1);
    EXPECT_EQ(context.request->timeframes_to_update[0].token_count, 1);

    EXPECT_EQ(context.request->timeframes_to_update[1].active_token_count, 0);
    EXPECT_EQ(context.request->timeframes_to_update[1].active_transaction_id,
              core::common::Uuid({0, 0}));
    EXPECT_EQ(context.request->timeframes_to_update[1].reporting_time,
              timeframe_bucket2);
    EXPECT_EQ(context.request->timeframes_to_update[1].token_count, 1);

    EXPECT_EQ(context.request->timeframes_to_update[2].active_token_count, 0);
    EXPECT_EQ(context.request->timeframes_to_update[2].active_transaction_id,
              core::common::Uuid({0, 0}));
    EXPECT_EQ(context.request->timeframes_to_update[2].reporting_time,
              timeframe_bucket3);
    EXPECT_EQ(context.request->timeframes_to_update[2].token_count, 1);

    // Update
    budget_key_timeframe1->active_token_count = 0;
    budget_key_timeframe2->active_token_count = 0;
    budget_key_timeframe3->active_token_count = 0;

    budget_key_timeframe1->token_count = 1;
    budget_key_timeframe2->token_count = 1;
    budget_key_timeframe3->token_count = 1;

    budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});

    context.result = SuccessExecutionResult();
    context.Finish();
    return SuccessExecutionResult();
  };

  NotifyBatchConsumeBudgetRequest notify_batch_consume_budget_request;
  notify_batch_consume_budget_request.time_buckets = {
      timeframe_bucket1, timeframe_bucket2, timeframe_bucket3};
  notify_batch_consume_budget_request.transaction_id = {1, 1};

  atomic<bool> condition = false;
  AsyncContext<NotifyBatchConsumeBudgetRequest,
               NotifyBatchConsumeBudgetResponse>
      notify_batch_consume_budget_context(
          make_shared<NotifyBatchConsumeBudgetRequest>(
              move(notify_batch_consume_budget_request)),
          [&](auto& notify_batch_consume_budget_context) {
            EXPECT_EQ(notify_batch_consume_budget_context.result,
                      SuccessExecutionResult());
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Notify(notify_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });

  EXPECT_EQ(budget_key_timeframe1->active_token_count, 0);
  EXPECT_EQ(budget_key_timeframe1->token_count, 1);
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({0, 0}));

  EXPECT_EQ(budget_key_timeframe2->active_token_count, 0);
  EXPECT_EQ(budget_key_timeframe2->token_count, 1);
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));

  EXPECT_EQ(budget_key_timeframe3->active_token_count, 0);
  EXPECT_EQ(budget_key_timeframe3->token_count, 1);
  EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
}

TEST(BatchConsumeBudgetTransactionProtocolTest, AbortInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  AbortBatchConsumeBudgetRequest abort_batch_consume_budget_request = {
      .time_buckets = {1, 100000000}};

  AsyncContext<AbortBatchConsumeBudgetRequest, AbortBatchConsumeBudgetResponse>
      abort_batch_consume_budget_context(
          make_shared<AbortBatchConsumeBudgetRequest>(
              move(abort_batch_consume_budget_request)),
          [&](auto& abort_batch_consume_budget_context) {
            // Will not be called
          });
  EXPECT_EQ(transaction_protocol->Abort(abort_batch_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));

  // Transaction ID is valid
  abort_batch_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(transaction_protocol->Abort(abort_batch_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(BatchConsumeBudgetTransactionProtocolTest, AbortInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  AbortBatchConsumeBudgetRequest abort_batch_consume_budget_request = {
      .transaction_id = {1, 1}, .time_buckets = {1, 100000000}};

  atomic<bool> condition = false;
  AsyncContext<AbortBatchConsumeBudgetRequest, AbortBatchConsumeBudgetResponse>
      abort_batch_consume_budget_context(
          make_shared<AbortBatchConsumeBudgetRequest>(
              move(abort_batch_consume_budget_request)),
          [&](auto& abort_batch_consume_budget_context) {
            EXPECT_EQ(abort_batch_consume_budget_context.result,
                      FailureExecutionResult(1234));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Abort(abort_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest, AbortRetry) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({3, 4});
  budget_key_timeframe1->token_count = 2;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe2->token_count = 2;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});
  budget_key_timeframe3->token_count = 2;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  AbortBatchConsumeBudgetRequest abort_batch_consume_budget_request = {
      .transaction_id = {1, 1},
      .time_buckets = {timeframe_bucket1, timeframe_bucket2,
                       timeframe_bucket3}};

  atomic<bool> condition = false;
  AsyncContext<AbortBatchConsumeBudgetRequest, AbortBatchConsumeBudgetResponse>
      abort_batch_consume_budget_context(
          make_shared<AbortBatchConsumeBudgetRequest>(
              move(abort_batch_consume_budget_request)),
          [&](auto& abort_batch_consume_budget_context) {
            EXPECT_EQ(abort_batch_consume_budget_context.result,
                      SuccessExecutionResult());
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Abort(abort_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetTransactionProtocolTest, Abort) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> update_invoked = false;
  auto transaction_protocol =
      make_shared<BatchConsumeBudgetTransactionProtocol>(budget_key_manager);

  uint64_t timeframe_bucket1 = nanoseconds(0).count();
  auto budget_key_timeframe1 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket1);
  budget_key_timeframe1->active_transaction_id = core::common::Uuid({1, 1});
  budget_key_timeframe1->token_count = 2;
  budget_key_timeframe1->active_token_count = 1;

  uint64_t timeframe_bucket2 = (nanoseconds(0) + hours(2)).count();
  auto budget_key_timeframe2 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket2);
  budget_key_timeframe2->active_transaction_id = core::common::Uuid({1, 1});
  budget_key_timeframe2->token_count = 2;
  budget_key_timeframe2->active_token_count = 1;

  uint64_t timeframe_bucket3 = (nanoseconds(0) + hours(5)).count();
  auto budget_key_timeframe3 =
      make_shared<BudgetKeyTimeframe>(timeframe_bucket3);
  budget_key_timeframe3->active_transaction_id = core::common::Uuid({1, 1});
  budget_key_timeframe3->token_count = 2;
  budget_key_timeframe3->active_token_count = 1;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe1, budget_key_timeframe2,
            budget_key_timeframe3};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->update_function = [&](auto& context) {
    update_invoked = true;
    // Verify
    EXPECT_EQ(context.request->timeframes_to_update.size(), 3);
    EXPECT_EQ(context.request->timeframes_to_update[0].active_token_count, 0);
    EXPECT_EQ(context.request->timeframes_to_update[0].active_transaction_id,
              core::common::Uuid({0, 0}));
    EXPECT_EQ(context.request->timeframes_to_update[0].reporting_time,
              timeframe_bucket1);
    EXPECT_EQ(context.request->timeframes_to_update[0].token_count, 2);

    EXPECT_EQ(context.request->timeframes_to_update[1].active_token_count, 0);
    EXPECT_EQ(context.request->timeframes_to_update[1].active_transaction_id,
              core::common::Uuid({0, 0}));
    EXPECT_EQ(context.request->timeframes_to_update[1].reporting_time,
              timeframe_bucket2);
    EXPECT_EQ(context.request->timeframes_to_update[1].token_count, 2);

    EXPECT_EQ(context.request->timeframes_to_update[2].active_token_count, 0);
    EXPECT_EQ(context.request->timeframes_to_update[2].active_transaction_id,
              core::common::Uuid({0, 0}));
    EXPECT_EQ(context.request->timeframes_to_update[2].reporting_time,
              timeframe_bucket3);
    EXPECT_EQ(context.request->timeframes_to_update[2].token_count, 2);

    // Update
    budget_key_timeframe1->active_token_count = 0;
    budget_key_timeframe2->active_token_count = 0;
    budget_key_timeframe3->active_token_count = 0;

    budget_key_timeframe1->token_count = 2;
    budget_key_timeframe2->token_count = 2;
    budget_key_timeframe3->token_count = 2;

    budget_key_timeframe1->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe2->active_transaction_id = core::common::Uuid({0, 0});
    budget_key_timeframe3->active_transaction_id = core::common::Uuid({0, 0});

    context.result = SuccessExecutionResult();
    context.Finish();
    return SuccessExecutionResult();
  };

  AbortBatchConsumeBudgetRequest abort_batch_consume_budget_request = {
      .transaction_id = {1, 1},
      .time_buckets = {timeframe_bucket1, timeframe_bucket2,
                       timeframe_bucket3}};

  atomic<bool> condition = false;
  AsyncContext<AbortBatchConsumeBudgetRequest, AbortBatchConsumeBudgetResponse>
      abort_batch_consume_budget_context(
          make_shared<AbortBatchConsumeBudgetRequest>(
              move(abort_batch_consume_budget_request)),
          [&](auto& abort_batch_consume_budget_context) {
            EXPECT_EQ(abort_batch_consume_budget_context.result,
                      SuccessExecutionResult());
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Abort(abort_batch_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });

  EXPECT_EQ(budget_key_timeframe1->active_token_count, 0);
  EXPECT_EQ(budget_key_timeframe1->token_count, 2);
  EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load(),
            core::common::Uuid({0, 0}));

  EXPECT_EQ(budget_key_timeframe2->active_token_count, 0);
  EXPECT_EQ(budget_key_timeframe2->token_count, 2);
  EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load(),
            core::common::Uuid({0, 0}));

  EXPECT_EQ(budget_key_timeframe3->active_token_count, 0);
  EXPECT_EQ(budget_key_timeframe3->token_count, 2);
  EXPECT_EQ(budget_key_timeframe3->active_transaction_id.load(),
            core::common::Uuid({0, 0}));
}
}  // namespace google::scp::pbs::test
