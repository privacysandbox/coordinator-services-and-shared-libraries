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

#include "pbs/transactions/src/batch_consume_budget_command.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/budget_key/mock/mock_budget_key.h"
#include "pbs/budget_key_provider/mock/mock_budget_key_provider.h"
#include "pbs/budget_key_transaction_protocols/mock/mock_batch_consume_budget_transaction_protocol.h"
#include "pbs/budget_key_transaction_protocols/src/error_codes.h"
#include "pbs/transactions/mock/mock_batch_consume_budget_command.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::JournalServiceInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TransactionCommandCallback;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::BatchConsumeBudgetCommand;
using google::scp::pbs::budget_key::mock::
    MockBatchConsumeBudgetTransactionProtocol;
using google::scp::pbs::budget_key::mock::MockBudgetKey;
using google::scp::pbs::budget_key_provider::mock::MockBudgetKeyProvider;
using google::scp::pbs::transactions::mock::MockBatchConsumeBudgetCommand;
using std::atomic;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using testing::UnorderedElementsAre;

/**
 * @brief This is a duplicate of the tests present in the
 * consume_budget_command_test except these tests are for
 * batch_consume_budget_command_test
 */
namespace google::scp::pbs::test {
TEST(BatchConsumeBudgetCommandTest, Prepare) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_key_provider = make_shared<MockBudgetKeyProvider>(
      mock_async_executor, journal_service, nosql_database_provider,
      mock_metric_client, mock_config_provider);
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    atomic<bool> condition(false);
    mock_key_provider->get_budget_key_mock =
        [&](core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                get_budget_key_context) {
          EXPECT_EQ(*get_budget_key_context.request->budget_key_name,
                    *budget_key_name);
          condition = true;
          return result;
        };
    shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
        mock_key_provider;

    MockBatchConsumeBudgetCommand consume_budget_command(
        transaction_id, budget_key_name, move(budget_consumptions),
        mock_async_executor, budget_key_provider);

    TransactionCommandCallback callback = [](ExecutionResult&) {};
    EXPECT_EQ(consume_budget_command.Prepare(callback),
              SuccessExecutionResult());
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnPrepareGetBudgetKeyCallbackFailure) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);
  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;
  get_budget_key_context.result = FailureExecutionResult(123);

  atomic<bool> condition(false);
  TransactionCommandCallback callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnPrepareGetBudgetKeyCallback(get_budget_key_context,
                                                       callback);
  WaitUntil([&]() { return condition.load(); });

  get_budget_key_context.result = RetryExecutionResult(123);

  condition = false;
  callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(RetryExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnPrepareGetBudgetKeyCallback(get_budget_key_context,
                                                       callback);
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetCommandTest,
     OnPrepareConsumeBudgetCallbackWithBudgetInsufficientFailures) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1, 2);
  budget_consumptions.emplace_back(2333, 20);
  budget_consumptions.emplace_back(214124, 212);

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);
  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<BudgetConsumptionRequestInfo> budgets;
  budgets.push_back({1, 2});
  budgets.push_back({2333, 20});
  budgets.push_back({214124, 212});

  AsyncContext<PrepareBatchConsumeBudgetRequest,
               PrepareBatchConsumeBudgetResponse>
      prepare_consume_budget_context;
  prepare_consume_budget_context.request =
      make_shared<PrepareBatchConsumeBudgetRequest>();
  prepare_consume_budget_context.result = FailureExecutionResult(
      core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET);
  prepare_consume_budget_context.request->budget_consumptions = move(budgets);
  prepare_consume_budget_context.response =
      make_shared<PrepareBatchConsumeBudgetResponse>();
  prepare_consume_budget_context.response->failed_budget_consumption_indices = {
      0, 2};

  TransactionCommandCallback callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result,
                ResultIs(FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET)));
  };

  consume_budget_command.OnPrepareBatchConsumeBudgetCallback(
      prepare_consume_budget_context, callback);

  EXPECT_THAT(
      consume_budget_command.GetFailedInsufficientBudgetConsumptions(),
      UnorderedElementsAre(ConsumeBudgetCommandRequestInfo(1, 2),
                           ConsumeBudgetCommandRequestInfo(214124, 212)));
}

TEST(BatchConsumeBudgetCommandTest, OnPrepareGetBudgetKeyCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
        get_budget_key_context;
    auto mock_budget_key = make_shared<MockBudgetKey>();
    auto transaction_protocol =
        make_shared<MockBatchConsumeBudgetTransactionProtocol>();
    transaction_protocol->prepare_mock =
        [&](core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                               PrepareBatchConsumeBudgetResponse>&
                prepare_consume_budget_context) {
          EXPECT_EQ(prepare_consume_budget_context.request->transaction_id,
                    transaction_id);
          EXPECT_EQ(prepare_consume_budget_context.request->budget_consumptions
                        .size(),
                    2);
          EXPECT_EQ(
              prepare_consume_budget_context.request->budget_consumptions[0]
                  .time_bucket,
              1000);
          EXPECT_EQ(
              prepare_consume_budget_context.request->budget_consumptions[0]
                  .token_count,
              10);
          EXPECT_EQ(
              prepare_consume_budget_context.request->budget_consumptions[1]
                  .time_bucket,
              10000000);
          EXPECT_EQ(
              prepare_consume_budget_context.request->budget_consumptions[1]
                  .token_count,
              100);
          return result;
        };
    mock_budget_key->batch_budget_consumption_transaction_protocol =
        transaction_protocol;

    get_budget_key_context.result = SuccessExecutionResult();
    get_budget_key_context.response = make_shared<GetBudgetKeyResponse>();
    get_budget_key_context.response->budget_key = mock_budget_key;

    atomic<bool> condition(false);
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          if (result == RetryExecutionResult(12345)) {
            EXPECT_THAT(execution_result,
                        ResultIs(FailureExecutionResult(
                            core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
          } else {
            EXPECT_THAT(execution_result, ResultIs(result));
          }
          condition = true;
        };

    consume_budget_command.OnPrepareGetBudgetKeyCallback(get_budget_key_context,
                                                         callback);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnPrepareConsumeBudgetCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<PrepareBatchConsumeBudgetRequest,
                 PrepareBatchConsumeBudgetResponse>
        prepare_consume_budget_context;
    prepare_consume_budget_context.result = result;
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          EXPECT_THAT(execution_result, ResultIs(result));
        };
    consume_budget_command.OnPrepareBatchConsumeBudgetCallback(
        prepare_consume_budget_context, callback);
  }
}

TEST(BatchConsumeBudgetCommandTest, Commit) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_key_provider = make_shared<MockBudgetKeyProvider>(
      mock_async_executor, journal_service, nosql_database_provider,
      mock_metric_client, mock_config_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    atomic<bool> condition(false);
    mock_key_provider->get_budget_key_mock =
        [&](core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                get_budget_key_context) {
          EXPECT_EQ(*get_budget_key_context.request->budget_key_name,
                    *budget_key_name);
          condition = true;
          return result;
        };
    shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
        mock_key_provider;

    MockBatchConsumeBudgetCommand consume_budget_command(
        transaction_id, budget_key_name, move(budget_consumptions),
        mock_async_executor, budget_key_provider);

    TransactionCommandCallback callback = [](ExecutionResult&) {};
    EXPECT_EQ(consume_budget_command.Commit(callback),
              SuccessExecutionResult());
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnCommitGetBudgetKeyCallbackFailure) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;
  get_budget_key_context.result = FailureExecutionResult(123);

  atomic<bool> condition(false);
  TransactionCommandCallback callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnCommitGetBudgetKeyCallback(get_budget_key_context,
                                                      callback);
  WaitUntil([&]() { return condition.load(); });

  get_budget_key_context.result = RetryExecutionResult(123);

  condition = false;
  callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(RetryExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnCommitGetBudgetKeyCallback(get_budget_key_context,
                                                      callback);
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetCommandTest, OnCommitGetBudgetKeyCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
        get_budget_key_context;
    auto mock_budget_key = make_shared<MockBudgetKey>();
    auto transaction_protocol =
        make_shared<MockBatchConsumeBudgetTransactionProtocol>();
    transaction_protocol->commit_mock =
        [&](core::AsyncContext<CommitBatchConsumeBudgetRequest,
                               CommitBatchConsumeBudgetResponse>&
                commit_consume_budget_context) {
          EXPECT_EQ(commit_consume_budget_context.request->transaction_id,
                    transaction_id);
          EXPECT_EQ(
              commit_consume_budget_context.request->budget_consumptions.size(),
              2);
          EXPECT_EQ(
              commit_consume_budget_context.request->budget_consumptions[0]
                  .time_bucket,
              1000);
          EXPECT_EQ(
              commit_consume_budget_context.request->budget_consumptions[0]
                  .token_count,
              10);
          EXPECT_EQ(
              commit_consume_budget_context.request->budget_consumptions[1]
                  .time_bucket,
              10000000);
          EXPECT_EQ(
              commit_consume_budget_context.request->budget_consumptions[1]
                  .token_count,
              100);
          return result;
        };
    mock_budget_key->batch_budget_consumption_transaction_protocol =
        transaction_protocol;

    get_budget_key_context.result = SuccessExecutionResult();
    get_budget_key_context.response = make_shared<GetBudgetKeyResponse>();
    get_budget_key_context.response->budget_key = mock_budget_key;

    atomic<bool> condition(false);
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          if (result == RetryExecutionResult(12345)) {
            EXPECT_THAT(execution_result,
                        ResultIs(FailureExecutionResult(
                            core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
          } else {
            EXPECT_THAT(execution_result, ResultIs(result));
          }
          condition = true;
        };

    consume_budget_command.OnCommitGetBudgetKeyCallback(get_budget_key_context,
                                                        callback);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnCommitConsumeBudgetCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);
  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<CommitBatchConsumeBudgetRequest,
                 CommitBatchConsumeBudgetResponse>
        commit_consume_budget_context;
    commit_consume_budget_context.result = result;
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          EXPECT_THAT(execution_result, ResultIs(result));
        };
    consume_budget_command.OnCommitBatchConsumeBudgetCallback(
        commit_consume_budget_context, callback);
  }
}

TEST(BatchConsumeBudgetCommandTest,
     OnCommitConsumeBudgetCallbackWithBudgetInsufficientFailures) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1, 2);
  budget_consumptions.emplace_back(2333, 20);
  budget_consumptions.emplace_back(214124, 212);

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);
  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<BudgetConsumptionRequestInfo> budgets;
  budgets.push_back({1, 2});
  budgets.push_back({2333, 20});
  budgets.push_back({214124, 212});

  AsyncContext<CommitBatchConsumeBudgetRequest,
               CommitBatchConsumeBudgetResponse>
      commit_consume_budget_context;
  commit_consume_budget_context.request =
      make_shared<CommitBatchConsumeBudgetRequest>();
  commit_consume_budget_context.result = FailureExecutionResult(
      core::errors::SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET);
  commit_consume_budget_context.request->budget_consumptions = move(budgets);
  commit_consume_budget_context.response =
      make_shared<CommitBatchConsumeBudgetResponse>();
  commit_consume_budget_context.response->failed_budget_consumption_indices = {
      0, 2};

  TransactionCommandCallback callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result,
                ResultIs(FailureExecutionResult(
                    core::errors::
                        SC_PBS_BUDGET_KEY_CONSUME_BUDGET_INSUFFICIENT_BUDGET)));
  };

  consume_budget_command.OnCommitBatchConsumeBudgetCallback(
      commit_consume_budget_context, callback);

  EXPECT_THAT(
      consume_budget_command.GetFailedInsufficientBudgetConsumptions(),
      UnorderedElementsAre(ConsumeBudgetCommandRequestInfo(1, 2),
                           ConsumeBudgetCommandRequestInfo(214124, 212)));
}

TEST(BatchConsumeBudgetCommandTest, Notify) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_key_provider = make_shared<MockBudgetKeyProvider>(
      mock_async_executor, journal_service, nosql_database_provider,
      mock_metric_client, mock_config_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    atomic<bool> condition(false);
    mock_key_provider->get_budget_key_mock =
        [&](core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                get_budget_key_context) {
          EXPECT_EQ(*get_budget_key_context.request->budget_key_name,
                    *budget_key_name);
          condition = true;
          return result;
        };
    shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
        mock_key_provider;

    MockBatchConsumeBudgetCommand consume_budget_command(
        transaction_id, budget_key_name, move(budget_consumptions),
        mock_async_executor, budget_key_provider);

    TransactionCommandCallback callback = [](ExecutionResult&) {};
    EXPECT_EQ(consume_budget_command.Notify(callback),
              SuccessExecutionResult());
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnNotifyGetBudgetKeyCallbackFailure) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;
  get_budget_key_context.result = FailureExecutionResult(123);

  atomic<bool> condition(false);
  TransactionCommandCallback callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnNotifyGetBudgetKeyCallback(get_budget_key_context,
                                                      callback);
  WaitUntil([&]() { return condition.load(); });

  get_budget_key_context.result = RetryExecutionResult(123);

  condition = false;
  callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(RetryExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnNotifyGetBudgetKeyCallback(get_budget_key_context,
                                                      callback);
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetCommandTest, OnNotifyGetBudgetKeyCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
        get_budget_key_context;
    auto mock_budget_key = make_shared<MockBudgetKey>();
    auto transaction_protocol =
        make_shared<MockBatchConsumeBudgetTransactionProtocol>();
    transaction_protocol->notify_mock =
        [&](core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                               NotifyBatchConsumeBudgetResponse>&
                notify_consume_budget_context) {
          EXPECT_EQ(notify_consume_budget_context.request->transaction_id,
                    transaction_id);
          EXPECT_EQ(notify_consume_budget_context.request->time_buckets.size(),
                    2);
          EXPECT_EQ(notify_consume_budget_context.request->time_buckets[0],
                    1000);
          EXPECT_EQ(notify_consume_budget_context.request->time_buckets[1],
                    10000000);
          return result;
        };
    mock_budget_key->batch_budget_consumption_transaction_protocol =
        transaction_protocol;

    get_budget_key_context.result = SuccessExecutionResult();
    get_budget_key_context.response = make_shared<GetBudgetKeyResponse>();
    get_budget_key_context.response->budget_key = mock_budget_key;

    atomic<bool> condition(false);
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          if (result == RetryExecutionResult(12345)) {
            EXPECT_THAT(execution_result,
                        ResultIs(FailureExecutionResult(
                            core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
          } else {
            EXPECT_THAT(execution_result, ResultIs(result));
          }
          condition = true;
        };

    consume_budget_command.OnNotifyGetBudgetKeyCallback(get_budget_key_context,
                                                        callback);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnNotifyConsumeBudgetCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<NotifyBatchConsumeBudgetRequest,
                 NotifyBatchConsumeBudgetResponse>
        notify_consume_budget_context;
    notify_consume_budget_context.result = result;
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          EXPECT_THAT(execution_result, ResultIs(result));
        };
    consume_budget_command.OnNotifyBatchConsumeBudgetCallback(
        notify_consume_budget_context, callback);
  }
}

TEST(BatchConsumeBudgetCommandTest, Abort) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_key_provider = make_shared<MockBudgetKeyProvider>(
      mock_async_executor, journal_service, nosql_database_provider,
      mock_metric_client, mock_config_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    atomic<bool> condition(false);
    mock_key_provider->get_budget_key_mock =
        [&](core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
                get_budget_key_context) {
          EXPECT_EQ(*get_budget_key_context.request->budget_key_name,
                    *budget_key_name);
          condition = true;
          return result;
        };
    shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
        mock_key_provider;

    MockBatchConsumeBudgetCommand consume_budget_command(
        transaction_id, budget_key_name, move(budget_consumptions),
        mock_async_executor, budget_key_provider);

    TransactionCommandCallback callback = [](ExecutionResult&) {};
    EXPECT_SUCCESS(consume_budget_command.Abort(callback));
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnAbortGetBudgetKeyCallbackFailure) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;
  get_budget_key_context.result = FailureExecutionResult(123);

  atomic<bool> condition(false);
  TransactionCommandCallback callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnAbortGetBudgetKeyCallback(get_budget_key_context,
                                                     callback);
  WaitUntil([&]() { return condition.load(); });

  get_budget_key_context.result = RetryExecutionResult(123);

  condition = false;
  callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(RetryExecutionResult(123)));
    condition = true;
  };
  consume_budget_command.OnAbortGetBudgetKeyCallback(get_budget_key_context,
                                                     callback);
  WaitUntil([&]() { return condition.load(); });
}

TEST(BatchConsumeBudgetCommandTest, OnAbortGetBudgetKeyCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
        get_budget_key_context;
    auto mock_budget_key = make_shared<MockBudgetKey>();
    auto transaction_protocol =
        make_shared<MockBatchConsumeBudgetTransactionProtocol>();
    transaction_protocol->abort_mock =
        [&](core::AsyncContext<AbortBatchConsumeBudgetRequest,
                               AbortBatchConsumeBudgetResponse>&
                abort_consume_budget_context) {
          EXPECT_EQ(abort_consume_budget_context.request->transaction_id,
                    transaction_id);
          EXPECT_EQ(abort_consume_budget_context.request->time_buckets.size(),
                    2);
          EXPECT_EQ(abort_consume_budget_context.request->time_buckets[0],
                    budget_consumptions[0].time_bucket);
          EXPECT_EQ(abort_consume_budget_context.request->time_buckets[1],
                    budget_consumptions[1].time_bucket);
          return result;
        };
    mock_budget_key->batch_budget_consumption_transaction_protocol =
        transaction_protocol;

    get_budget_key_context.result = SuccessExecutionResult();
    get_budget_key_context.response = make_shared<GetBudgetKeyResponse>();
    get_budget_key_context.response->budget_key = mock_budget_key;

    atomic<bool> condition(false);
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          if (result == RetryExecutionResult(12345)) {
            EXPECT_THAT(execution_result,
                        ResultIs(FailureExecutionResult(
                            core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
          } else {
            EXPECT_THAT(execution_result, ResultIs(result));
          }
          condition = true;
        };

    consume_budget_command.OnAbortGetBudgetKeyCallback(get_budget_key_context,
                                                       callback);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BatchConsumeBudgetCommandTest, OnAbortConsumeBudgetCallback) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    AsyncContext<AbortBatchConsumeBudgetRequest,
                 AbortBatchConsumeBudgetResponse>
        abort_consume_budget_context;
    abort_consume_budget_context.result = result;
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          EXPECT_THAT(execution_result, ResultIs(result));
        };
    consume_budget_command.OnAbortBatchConsumeBudgetCallback(
        abort_consume_budget_context, callback);
  }
}

TEST(BatchConsumeBudgetCommandTest, VerifyGetters) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(1000, 10);
  budget_consumptions.emplace_back(10000000, 100);

  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider =
      make_shared<MockBudgetKeyProvider>(
          mock_async_executor, journal_service, nosql_database_provider,
          mock_metric_client, mock_config_provider);

  MockBatchConsumeBudgetCommand consume_budget_command(
      transaction_id, budget_key_name, move(budget_consumptions),
      mock_async_executor, budget_key_provider);
  EXPECT_EQ(*consume_budget_command.GetBudgetKeyName(), *budget_key_name);
  EXPECT_EQ(consume_budget_command.GetBudgetConsumptions().size(), 2);
  EXPECT_EQ(consume_budget_command.GetBudgetConsumptions()[0].time_bucket,
            1000);
  EXPECT_EQ(consume_budget_command.GetBudgetConsumptions()[0].token_count, 10);
  EXPECT_EQ(consume_budget_command.GetBudgetConsumptions()[1].time_bucket,
            10000000);
  EXPECT_EQ(consume_budget_command.GetBudgetConsumptions()[1].token_count, 100);
  EXPECT_EQ(consume_budget_command.GetVersion().major, 1);
  EXPECT_EQ(consume_budget_command.GetVersion().minor, 0);
}

}  // namespace google::scp::pbs::test
