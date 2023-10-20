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

#include "pbs/budget_key_transaction_protocols/src/consume_budget_transaction_protocol.h"

#include <gtest/gtest.h>

#include <atomic>
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/budget_key_timeframe_manager/mock/mock_budget_key_timeframe_manager.h"
#include "pbs/budget_key_transaction_protocols/mock/mock_consume_budget_transaction_protocol_with_overrides.h"
#include "pbs/budget_key_transaction_protocols/src/error_codes.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "public/core/interface/execution_result.h"
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
using std::vector;

namespace google::scp::pbs::test {
TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetPrepareInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  PrepareConsumeBudgetRequest prepare_consume_budget_request{.time_bucket = 0,
                                                             .token_count = 0};

  AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>
      prepare_consume_budget_context(make_shared<PrepareConsumeBudgetRequest>(
                                         move(prepare_consume_budget_request)),
                                     [&](auto& prepare_consume_budget_context) {
                                       // Will not be called
                                     });
  EXPECT_EQ(transaction_protocol->Prepare(prepare_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));
  prepare_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(transaction_protocol->Prepare(prepare_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetPrepareInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareConsumeBudgetRequest prepare_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0, .token_count = 0};

  AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>
      prepare_consume_budget_context(
          make_shared<PrepareConsumeBudgetRequest>(
              move(prepare_consume_budget_request)),
          [&](auto& prepare_consume_budget_context) {
            EXPECT_EQ(prepare_consume_budget_context.result,
                      FailureExecutionResult(1234));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetPrepareActiveTransactionInProgress) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  core::common::Uuid uuid{.high = 1, .low = 2};
  budget_key_timeframe->active_transaction_id = uuid;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareConsumeBudgetRequest prepare_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0, .token_count = 0};

  AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>
      prepare_consume_budget_context(
          make_shared<PrepareConsumeBudgetRequest>(
              move(prepare_consume_budget_request)),
          [&](auto& prepare_consume_budget_context) {
            EXPECT_EQ(
                prepare_consume_budget_context.result,
                RetryExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(), uuid);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetPrepareInSufficientToken) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  budget_key_timeframe->token_count = 10;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareConsumeBudgetRequest prepare_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0, .token_count = 20};

  AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>
      prepare_consume_budget_context(
          make_shared<PrepareConsumeBudgetRequest>(
              move(prepare_consume_budget_request)),
          [&](auto& prepare_consume_budget_context) {
            EXPECT_EQ(
                prepare_consume_budget_context.result,
                FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 10);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 0);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetPrepareSufficientToken) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  budget_key_timeframe->token_count = 10;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  PrepareConsumeBudgetRequest prepare_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0, .token_count = 10};

  AsyncContext<PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse>
      prepare_consume_budget_context(
          make_shared<PrepareConsumeBudgetRequest>(
              move(prepare_consume_budget_request)),
          [&](auto& prepare_consume_budget_context) {
            EXPECT_EQ(prepare_consume_budget_context.result,
                      SuccessExecutionResult());
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Prepare(prepare_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 10);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 0);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetCommitInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  CommitConsumeBudgetRequest commit_consume_budget_request{.time_bucket = 0,
                                                           .token_count = 0};

  AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
      commit_consume_budget_context(make_shared<CommitConsumeBudgetRequest>(
                                        move(commit_consume_budget_request)),
                                    [&](auto& commit_consume_budget_context) {
                                      // Will not be called
                                    });
  EXPECT_EQ(transaction_protocol->Commit(commit_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));
  commit_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(transaction_protocol->Commit(commit_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetCommitInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  CommitConsumeBudgetRequest commit_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0, .token_count = 0};

  AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
      commit_consume_budget_context(make_shared<CommitConsumeBudgetRequest>(
                                        move(commit_consume_budget_request)),
                                    [&](auto& commit_consume_budget_context) {
                                      EXPECT_EQ(
                                          commit_consume_budget_context.result,
                                          FailureExecutionResult(1234));
                                      condition = true;
                                    });

  EXPECT_EQ(transaction_protocol->Commit(commit_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetCommitActiveTransactionInProgress) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  core::common::Uuid uuid{.high = 1, .low = 2};
  budget_key_timeframe->active_transaction_id = uuid;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  CommitConsumeBudgetRequest commit_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0, .token_count = 0};

  AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
      commit_consume_budget_context(
          make_shared<CommitConsumeBudgetRequest>(
              move(commit_consume_budget_request)),
          [&](auto& commit_consume_budget_context) {
            EXPECT_EQ(
                commit_consume_budget_context.result,
                RetryExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_ACTIVE_TRANSACTION_IN_PROGRESS));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Commit(commit_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(), uuid);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetCommitActiveTransactionInProgressWithSameId) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  core::common::Uuid uuid{.high = 1, .low = 2};
  budget_key_timeframe->active_transaction_id = uuid;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  CommitConsumeBudgetRequest commit_consume_budget_request{
      .transaction_id{1, 2}, .time_bucket = 0, .token_count = 0};

  AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
      commit_consume_budget_context(make_shared<CommitConsumeBudgetRequest>(
                                        move(commit_consume_budget_request)),
                                    [&](auto& commit_consume_budget_context) {
                                      EXPECT_EQ(
                                          commit_consume_budget_context.result,
                                          SuccessExecutionResult());
                                      condition = true;
                                    });

  EXPECT_EQ(transaction_protocol->Commit(commit_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(), uuid);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetCommitInSufficientToken) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  budget_key_timeframe->token_count = 10;

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  CommitConsumeBudgetRequest commit_consume_budget_request{
      .transaction_id{2, 1}, .time_bucket = 0, .token_count = 20};

  AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
      commit_consume_budget_context(
          make_shared<CommitConsumeBudgetRequest>(
              move(commit_consume_budget_request)),
          [&](auto& commit_consume_budget_context) {
            EXPECT_EQ(
                commit_consume_budget_context.result,
                FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET));
            condition = true;
          });

  EXPECT_EQ(transaction_protocol->Commit(commit_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 10);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 2);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 1);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetCommitSufficientToken) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  budget_key_timeframe->token_count = 25;

  budget_key_manager->update_function =
      [](core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                            UpdateBudgetKeyTimeframeResponse>& context) {
        EXPECT_EQ(
            context.request->timeframes_to_update.back().active_token_count,
            15);
        EXPECT_EQ(context.request->timeframes_to_update.back()
                      .active_transaction_id.high,
                  0);
        EXPECT_EQ(context.request->timeframes_to_update.back()
                      .active_transaction_id.low,
                  1);
        EXPECT_EQ(context.request->timeframes_to_update.back().reporting_time,
                  0);
        EXPECT_EQ(context.request->timeframes_to_update.back().token_count, 25);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  CommitConsumeBudgetRequest commit_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0, .token_count = 10};

  AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
      commit_consume_budget_context(make_shared<CommitConsumeBudgetRequest>(
                                        move(commit_consume_budget_request)),
                                    [&](auto& commit_consume_budget_context) {
                                      EXPECT_EQ(
                                          commit_consume_budget_context.result,
                                          SuccessExecutionResult());
                                      condition = true;
                                    });

  EXPECT_EQ(transaction_protocol->Commit(commit_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 25);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 1);
  EXPECT_EQ(budget_key_timeframe->active_token_count.load(), 15);
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetNotifyInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  NotifyConsumeBudgetRequest notify_consume_budget_request{
      .time_bucket = 0,
  };

  AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>
      notify_consume_budget_context(make_shared<NotifyConsumeBudgetRequest>(
                                        move(notify_consume_budget_request)),
                                    [&](auto& notify_consume_budget_context) {
                                      // Will not be called
                                    });
  EXPECT_EQ(transaction_protocol->Notify(notify_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));
  notify_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(transaction_protocol->Notify(notify_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetNotifyInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  NotifyConsumeBudgetRequest notify_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0};

  AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>
      notify_consume_budget_context(make_shared<NotifyConsumeBudgetRequest>(
                                        move(notify_consume_budget_request)),
                                    [&](auto& notify_consume_budget_context) {
                                      EXPECT_EQ(
                                          notify_consume_budget_context.result,
                                          FailureExecutionResult(1234));
                                      condition = true;
                                    });

  EXPECT_EQ(transaction_protocol->Notify(notify_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetNotifyActiveTransactionIdMismatch) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  core::common::Uuid uuid{.high = 1, .low = 2};
  budget_key_timeframe->active_transaction_id = uuid;

  budget_key_manager->update_function =
      [](core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                            UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        EXPECT_EQ(false, true);
        return SuccessExecutionResult();
      };

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  NotifyConsumeBudgetRequest notify_consume_budget_request{
      .transaction_id{0, 1}, .time_bucket = 0};

  AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>
      notify_consume_budget_context(make_shared<NotifyConsumeBudgetRequest>(
                                        move(notify_consume_budget_request)),
                                    [&](auto& notify_consume_budget_context) {
                                      EXPECT_EQ(
                                          notify_consume_budget_context.result,
                                          SuccessExecutionResult());
                                      condition = true;
                                    });

  EXPECT_EQ(transaction_protocol->Notify(notify_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(), uuid);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetNotifyAborted) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  budget_key_timeframe->token_count = 22;
  budget_key_timeframe->active_transaction_id = {1, 1};
  budget_key_timeframe->active_token_count = 2;

  budget_key_manager->update_function =
      [](core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                            UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .active_token_count,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .active_transaction_id.high,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .active_transaction_id.low,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .reporting_time,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .token_count,
            22);

        update_budget_key_timeframe_context.result = SuccessExecutionResult();
        update_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  AbortConsumeBudgetRequest abort_consume_budget_request{.transaction_id{1, 1},
                                                         .time_bucket = 0};

  AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>
      abort_consume_budget_context(make_shared<AbortConsumeBudgetRequest>(
                                       move(abort_consume_budget_request)),
                                   [&](auto& abort_consume_budget_context) {
                                     EXPECT_EQ(
                                         abort_consume_budget_context.result,
                                         SuccessExecutionResult());
                                     condition = true;
                                   });

  EXPECT_EQ(transaction_protocol->Abort(abort_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 22);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 0);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetNotify) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  budget_key_timeframe->token_count = 22;
  budget_key_timeframe->active_transaction_id = {1, 1};
  budget_key_timeframe->active_token_count = 2;

  budget_key_manager->update_function =
      [](core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                            UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .active_token_count,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .active_transaction_id.high,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .active_transaction_id.low,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .reporting_time,
            0);
        EXPECT_EQ(
            update_budget_key_timeframe_context.request->timeframes_to_update
                .back()
                .token_count,
            2);

        update_budget_key_timeframe_context.result = SuccessExecutionResult();
        update_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  NotifyConsumeBudgetRequest notify_consume_budget_request{
      .transaction_id{1, 1}, .time_bucket = 0};

  AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>
      notify_consume_budget_context(make_shared<NotifyConsumeBudgetRequest>(
                                        move(notify_consume_budget_request)),
                                    [&](auto& notify_consume_budget_context) {
                                      EXPECT_EQ(
                                          notify_consume_budget_context.result,
                                          SuccessExecutionResult());
                                      condition = true;
                                    });

  EXPECT_EQ(transaction_protocol->Notify(notify_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 2);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 0);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetAbortInvalidLoad) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function = [&](auto&) {
    return FailureExecutionResult(1234);
  };

  AbortConsumeBudgetRequest abort_consume_budget_request{
      .time_bucket = 0,
  };

  AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>
      abort_consume_budget_context(make_shared<AbortConsumeBudgetRequest>(
                                       move(abort_consume_budget_request)),
                                   [&](auto& abort_consume_budget_context) {
                                     // Will not be called
                                   });
  EXPECT_EQ(transaction_protocol->Abort(abort_consume_budget_context),
            FailureExecutionResult(
                core::errors::SC_PBS_BUDGET_KEY_INVALID_TRANSACTION_ID));
  abort_consume_budget_context.request->transaction_id = {1, 1};
  EXPECT_EQ(transaction_protocol->Abort(abort_consume_budget_context),
            FailureExecutionResult(1234));
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetAbortInvalidTimeframe) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.result = FailureExecutionResult(1234);
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  AbortConsumeBudgetRequest abort_consume_budget_request{.transaction_id{0, 1},
                                                         .time_bucket = 0};

  AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>
      abort_consume_budget_context(make_shared<AbortConsumeBudgetRequest>(
                                       move(abort_consume_budget_request)),
                                   [&](auto& abort_consume_budget_context) {
                                     EXPECT_EQ(
                                         abort_consume_budget_context.result,
                                         FailureExecutionResult(1234));
                                     condition = true;
                                   });

  EXPECT_EQ(transaction_protocol->Abort(abort_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST(ConsumeBudgetTransactionProtocolTest,
     ConsumeBudgetAbortActiveTransactionIdMismatch) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  core::common::Uuid uuid{.high = 1, .low = 2};
  budget_key_timeframe->active_transaction_id = uuid;

  budget_key_manager->update_function =
      [](core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                            UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        EXPECT_EQ(false, true);
        return SuccessExecutionResult();
      };

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  AbortConsumeBudgetRequest abort_consume_budget_request{.transaction_id{0, 1},
                                                         .time_bucket = 0};

  AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>
      abort_consume_budget_context(make_shared<AbortConsumeBudgetRequest>(
                                       move(abort_consume_budget_request)),
                                   [&](auto& abort_consume_budget_context) {
                                     EXPECT_EQ(
                                         abort_consume_budget_context.result,
                                         SuccessExecutionResult());
                                     condition = true;
                                   });

  EXPECT_EQ(transaction_protocol->Abort(abort_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(), uuid);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest, ConsumeBudgetAbort) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  atomic<bool> condition(false);
  auto transaction_protocol =
      make_shared<ConsumeBudgetTransactionProtocol>(budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(0);
  budget_key_timeframe->token_count = 22;
  budget_key_timeframe->active_transaction_id = {1, 1};
  budget_key_timeframe->active_token_count = 2;

  budget_key_manager->update_function =
      [](core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                            UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        update_budget_key_timeframe_context.result = SuccessExecutionResult();
        update_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  budget_key_manager->load_function =
      [&](auto& load_budget_key_timeframe_context) {
        load_budget_key_timeframe_context.response =
            make_shared<LoadBudgetKeyTimeframeResponse>();
        load_budget_key_timeframe_context.response->budget_key_frames = {
            budget_key_timeframe};

        load_budget_key_timeframe_context.result = SuccessExecutionResult();
        load_budget_key_timeframe_context.Finish();
        return SuccessExecutionResult();
      };

  AbortConsumeBudgetRequest abort_consume_budget_request{.transaction_id{1, 1},
                                                         .time_bucket = 0};

  AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>
      abort_consume_budget_context(make_shared<AbortConsumeBudgetRequest>(
                                       move(abort_consume_budget_request)),
                                   [&](auto& abort_consume_budget_context) {
                                     EXPECT_EQ(
                                         abort_consume_budget_context.result,
                                         SuccessExecutionResult());
                                     condition = true;
                                   });

  EXPECT_EQ(transaction_protocol->Abort(abort_consume_budget_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe->time_bucket_index, 0);
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 22);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 0);
  EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
}

TEST(ConsumeBudgetTransactionProtocolTest, OnCommitLogged) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocolWithOverrides>(
          budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(01);
  Uuid active_transaction_id = Uuid::GenerateUuid();
  budget_key_timeframe->token_count = 2;
  budget_key_timeframe->active_token_count = 3;
  budget_key_timeframe->active_transaction_id = core::common::kZeroUuid;

  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(234),
                                     SuccessExecutionResult()};

  for (auto result : results) {
    AsyncContext<CommitConsumeBudgetRequest, CommitConsumeBudgetResponse>
        commit_consume_budget_context;

    commit_consume_budget_context.callback =
        [&](AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
                commit_consume_budget_context) {
          EXPECT_THAT(commit_consume_budget_context.result, ResultIs(result));
          EXPECT_EQ(budget_key_timeframe->time_bucket_index, 1);
          EXPECT_EQ(budget_key_timeframe->token_count, 2);

          if (!result.Successful()) {
            EXPECT_EQ(budget_key_timeframe->active_token_count, 3);
            EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(),
                      core::common::kZeroUuid);
          } else {
            EXPECT_EQ(budget_key_timeframe->active_token_count, 100);
            EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(),
                      active_transaction_id);
          }
        };

    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>
        update_budget_key_timeframe_context;
    update_budget_key_timeframe_context.request =
        make_shared<UpdateBudgetKeyTimeframeRequest>();
    update_budget_key_timeframe_context.request->timeframes_to_update
        .emplace_back();

    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .active_token_count = 100;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .active_transaction_id = active_transaction_id;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .reporting_time = 1;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .token_count = 2;

    update_budget_key_timeframe_context.result = result;
    transaction_protocol->OnCommitLogged(budget_key_timeframe,
                                         commit_consume_budget_context,
                                         update_budget_key_timeframe_context);
  }
}

TEST(ConsumeBudgetTransactionProtocolTest, OnCommitNotifyLogged) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocolWithOverrides>(
          budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(01);
  Uuid active_transaction_id = Uuid::GenerateUuid();
  budget_key_timeframe->token_count = 2;
  budget_key_timeframe->active_token_count = 3;
  budget_key_timeframe->active_transaction_id = active_transaction_id;

  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(234),
                                     SuccessExecutionResult()};

  for (auto result : results) {
    AsyncContext<NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse>
        notify_consume_budget_context;

    notify_consume_budget_context.callback =
        [&](AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
                notify_consume_budget_context) {
          EXPECT_THAT(notify_consume_budget_context.result, ResultIs(result));
          EXPECT_EQ(budget_key_timeframe->time_bucket_index, 1);

          if (!result.Successful()) {
            EXPECT_EQ(budget_key_timeframe->token_count, 2);
            EXPECT_EQ(budget_key_timeframe->active_token_count, 3);
            EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(),
                      active_transaction_id);
          } else {
            EXPECT_EQ(budget_key_timeframe->token_count, 10);
            EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
            EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(),
                      core::common::kZeroUuid);
          }
        };

    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>
        update_budget_key_timeframe_context;
    update_budget_key_timeframe_context.request =
        make_shared<UpdateBudgetKeyTimeframeRequest>();
    update_budget_key_timeframe_context.request->timeframes_to_update
        .emplace_back();

    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .active_token_count = 0;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .active_transaction_id = core::common::kZeroUuid;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .reporting_time = 1;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .token_count = 10;

    update_budget_key_timeframe_context.result = result;
    transaction_protocol->OnNotifyLogged(budget_key_timeframe,
                                         notify_consume_budget_context,
                                         update_budget_key_timeframe_context);
  }
}

TEST(ConsumeBudgetTransactionProtocolTest, OnAbortLogged) {
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocolWithOverrides>(
          budget_key_manager);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(01);
  Uuid active_transaction_id = Uuid::GenerateUuid();
  budget_key_timeframe->token_count = 2;
  budget_key_timeframe->active_token_count = 12;
  budget_key_timeframe->active_transaction_id = active_transaction_id;

  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(234),
                                     SuccessExecutionResult()};

  for (auto result : results) {
    AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>
        abort_consume_budget_context;
    abort_consume_budget_context.request =
        make_shared<AbortConsumeBudgetRequest>();
    abort_consume_budget_context.request->transaction_id =
        active_transaction_id;

    abort_consume_budget_context.callback =
        [&](AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
                abort_consume_budget_context) {
          EXPECT_THAT(abort_consume_budget_context.result, ResultIs(result));
          EXPECT_EQ(budget_key_timeframe->time_bucket_index, 1);
          EXPECT_EQ(budget_key_timeframe->token_count, 2);

          if (!result.Successful()) {
            EXPECT_EQ(budget_key_timeframe->active_token_count, 12);
            EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(),
                      active_transaction_id);
          } else {
            EXPECT_EQ(budget_key_timeframe->active_token_count, 0);
            EXPECT_EQ(budget_key_timeframe->active_transaction_id.load(),
                      core::common::kZeroUuid);
          }
        };

    AsyncContext<UpdateBudgetKeyTimeframeRequest,
                 UpdateBudgetKeyTimeframeResponse>
        update_budget_key_timeframe_context;
    update_budget_key_timeframe_context.request =
        make_shared<UpdateBudgetKeyTimeframeRequest>();
    update_budget_key_timeframe_context.request->timeframes_to_update
        .emplace_back();

    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .active_token_count = 0;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .active_transaction_id = core::common::kZeroUuid;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .reporting_time = 1;
    update_budget_key_timeframe_context.request->timeframes_to_update.back()
        .token_count = 2;

    update_budget_key_timeframe_context.result = result;

    transaction_protocol->OnAbortLogged(budget_key_timeframe,
                                        abort_consume_budget_context,
                                        update_budget_key_timeframe_context);
  }
}

}  // namespace google::scp::pbs::test
