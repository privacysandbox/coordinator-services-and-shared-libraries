
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
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/error_codes.h"
#include "pbs/pbs_client/mock/mock_pbs_client.h"
#include "pbs/pbs_client/mock/transactional/mock_client_consume_budget_command.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionCommandCallback;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Uuid;
using google::scp::core::test::ResultIs;
using google::scp::pbs::client::mock::MockClientConsumeBudgetCommand;
using google::scp::pbs::client::mock::MockPrivacyBudgetServiceClient;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::pbs::test {
TEST(PBSClientConsumeBudgetCommandTest, Begin) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  bool is_called = false;
  mock_client->initiate_consume_budget_transaction_mock =
      [&](AsyncContext<ConsumeBudgetTransactionRequest,
                       ConsumeBudgetTransactionResponse>& context) {
        EXPECT_EQ(context.request->budget_keys->size(), 1);
        EXPECT_EQ(*context.request->budget_keys->at(0).budget_key_name,
                  *metadata.budget_key_name);
        EXPECT_EQ(context.request->budget_keys->at(0).time_bucket,
                  metadata.time_bucket);
        EXPECT_EQ(context.request->budget_keys->at(0).token_count,
                  metadata.token_count);
        EXPECT_EQ(context.request->transaction_id, transaction_id);
        EXPECT_EQ(*context.request->transaction_secret, *transaction_secret);
        is_called = true;
        return SuccessExecutionResult();
      };
  TransactionCommandCallback callback;
  EXPECT_EQ(client_consume_budget_command.Begin(callback),
            SuccessExecutionResult());
  EXPECT_EQ(is_called, true);
}

TEST(PBSClientConsumeBudgetCommandTest,
     OnInitiateConsumeBudgetTransactionCallback) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  client_consume_budget_command.GetLastExecutionTimestamp() = 654321;
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.response =
      make_shared<ConsumeBudgetTransactionResponse>();
  consume_budget_transaction_context.response->last_execution_timestamp =
      123456;
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234),
                                     SuccessExecutionResult()};
  for (auto result : results) {
    bool is_called = false;
    consume_budget_transaction_context.result = result;
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          EXPECT_THAT(execution_result, ResultIs(result));
          is_called = true;
        };
    client_consume_budget_command.OnInitiateConsumeBudgetTransactionCallback(
        consume_budget_transaction_context, callback);
    EXPECT_EQ(is_called, true);
    if (!result.Successful()) {
      EXPECT_EQ(client_consume_budget_command.GetLastExecutionTimestamp(),
                654321);
    } else {
      EXPECT_EQ(client_consume_budget_command.GetLastExecutionTimestamp(),
                123456);
    }
  }
}

TEST(PBSClientConsumeBudgetCommandTest, ExecuteTransactionPhase) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  client_consume_budget_command.GetLastExecutionTimestamp() = 12345;
  vector<TransactionExecutionPhase> phases = {
      TransactionExecutionPhase::Begin,  TransactionExecutionPhase::Prepare,
      TransactionExecutionPhase::Commit, TransactionExecutionPhase::Notify,
      TransactionExecutionPhase::Abort,  TransactionExecutionPhase::End,
      TransactionExecutionPhase::Unknown};
  for (auto phase : phases) {
    bool is_called = false;
    mock_client->execute_transaction_phase_mock =
        [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                context) {
          EXPECT_EQ(context.request->transaction_id, transaction_id);
          EXPECT_EQ(*context.request->transaction_secret, *transaction_secret);
          EXPECT_EQ(context.request->last_execution_timestamp, 12345);
          EXPECT_EQ(context.request->transaction_execution_phase, phase);
          is_called = true;
          return SuccessExecutionResult();
        };
    TransactionCommandCallback callback;
    EXPECT_EQ(
        client_consume_budget_command.ExecuteTransactionPhase(phase, callback),
        SuccessExecutionResult());
    EXPECT_EQ(is_called, true);
  }
}

TEST(PBSClientConsumeBudgetCommandTest, OnExecuteTransactionPhaseCallback) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  client_consume_budget_command.GetLastExecutionTimestamp() = 12345;
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.response = make_shared<TransactionPhaseResponse>();
  transaction_phase_context.response->last_execution_timestamp = 654321;
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234),
                                     SuccessExecutionResult()};
  for (auto result : results) {
    bool is_called = false;
    transaction_phase_context.result = result;
    TransactionCommandCallback callback =
        [&](ExecutionResult& execution_result) {
          EXPECT_THAT(execution_result, ResultIs(result));
          is_called = true;
        };
    client_consume_budget_command.OnPhaseExecutionCallback(
        transaction_phase_context, callback);
    EXPECT_EQ(is_called, true);
    if (!result.Successful()) {
      EXPECT_EQ(client_consume_budget_command.GetLastExecutionTimestamp(),
                12345);
    } else {
      EXPECT_EQ(client_consume_budget_command.GetLastExecutionTimestamp(),
                654321);
    }
  }
}

TEST(PBSClientConsumeBudgetCommandTest,
     OnExecuteTransactionPhaseCallbackPreConditionFailed) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  client_consume_budget_command.GetLastExecutionTimestamp() = 12345;
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Prepare;
  transaction_phase_context.request->transaction_secret =
      make_shared<string>("secret!0");
  bool is_called = false;
  transaction_phase_context.result = FailureExecutionResult(
      core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_PRECONDITION_FAILED);
  TransactionCommandCallback callback = [&](ExecutionResult& execution_result) {
    EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(123)));
    is_called = true;
  };
  mock_client->get_transaction_status_mock =
      [](AsyncContext<GetTransactionStatusRequest,
                      GetTransactionStatusResponse>&
             get_transaction_status_context) {
        return FailureExecutionResult(123);
      };
  client_consume_budget_command.OnPhaseExecutionCallback(
      transaction_phase_context, callback);
  EXPECT_EQ(is_called, true);
  is_called = false;
  mock_client->get_transaction_status_mock =
      [](AsyncContext<GetTransactionStatusRequest,
                      GetTransactionStatusResponse>&
             get_transaction_status_context) {
        get_transaction_status_context.result = SuccessExecutionResult();
        get_transaction_status_context.response =
            make_shared<GetTransactionStatusResponse>();
        get_transaction_status_context.response->last_execution_timestamp =
            123456789;
        get_transaction_status_context.Finish();
        return SuccessExecutionResult();
      };
  client_consume_budget_command.execute_transaction_phase_mock =
      [&](TransactionExecutionPhase phase, TransactionCommandCallback&) {
        EXPECT_EQ(phase, TransactionExecutionPhase::Prepare);
        is_called = true;
        return SuccessExecutionResult();
      };
  client_consume_budget_command.OnPhaseExecutionCallback(
      transaction_phase_context, callback);
  EXPECT_EQ(is_called, true);
  EXPECT_EQ(client_consume_budget_command.GetLastExecutionTimestamp(),
            123456789);
}

TEST(PBSClientConsumeBudgetCommandTest, Prepare) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234),
                                     SuccessExecutionResult()};
  for (auto result : results) {
    bool is_called = false;
    client_consume_budget_command.execute_transaction_phase_mock =
        [&](TransactionExecutionPhase phase, TransactionCommandCallback&) {
          EXPECT_EQ(phase, TransactionExecutionPhase::Prepare);
          is_called = true;
          return result;
        };
    TransactionCommandCallback callback;
    EXPECT_THAT(client_consume_budget_command.Prepare(callback),
                ResultIs(result));
  }
}

TEST(PBSClientConsumeBudgetCommandTest, Commit) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234),
                                     SuccessExecutionResult()};
  for (auto result : results) {
    bool is_called = false;
    client_consume_budget_command.execute_transaction_phase_mock =
        [&](TransactionExecutionPhase phase, TransactionCommandCallback&) {
          EXPECT_EQ(phase, TransactionExecutionPhase::Commit);
          is_called = true;
          return result;
        };
    TransactionCommandCallback callback;
    EXPECT_THAT(client_consume_budget_command.Commit(callback),
                ResultIs(result));
  }
}

TEST(PBSClientConsumeBudgetCommandTest, Notify) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234),
                                     SuccessExecutionResult()};
  for (auto result : results) {
    bool is_called = false;
    client_consume_budget_command.execute_transaction_phase_mock =
        [&](TransactionExecutionPhase phase, TransactionCommandCallback&) {
          EXPECT_EQ(phase, TransactionExecutionPhase::Notify);
          is_called = true;
          return result;
        };
    TransactionCommandCallback callback;
    EXPECT_THAT(client_consume_budget_command.Notify(callback),
                ResultIs(result));
  }
}

TEST(PBSClientConsumeBudgetCommandTest, Abort) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234),
                                     SuccessExecutionResult()};
  for (auto result : results) {
    bool is_called = false;
    client_consume_budget_command.execute_transaction_phase_mock =
        [&](TransactionExecutionPhase phase, TransactionCommandCallback&) {
          EXPECT_EQ(phase, TransactionExecutionPhase::Abort);
          is_called = true;
          return result;
        };
    TransactionCommandCallback callback;
    EXPECT_THAT(client_consume_budget_command.Abort(callback),
                ResultIs(result));
  }
}

TEST(PBSClientConsumeBudgetCommandTest, End) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> async_executor = mock_async_executor;
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction_secret = make_shared<string>("this is secret");
  auto mock_client = make_shared<MockPrivacyBudgetServiceClient>();
  shared_ptr<PrivacyBudgetServiceClientInterface> client = mock_client;
  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);
  MockClientConsumeBudgetCommand client_consume_budget_command(
      transaction_id, transaction_secret, budget_keys, async_executor, client);
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234),
                                     SuccessExecutionResult()};
  for (auto result : results) {
    bool is_called = false;
    client_consume_budget_command.execute_transaction_phase_mock =
        [&](TransactionExecutionPhase phase, TransactionCommandCallback&) {
          EXPECT_EQ(phase, TransactionExecutionPhase::End);
          is_called = true;
          return result;
        };
    TransactionCommandCallback callback;
    EXPECT_THAT(client_consume_budget_command.End(callback), ResultIs(result));
  }
}
}  // namespace google::scp::pbs::test
