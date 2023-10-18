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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/journal_service/mock/mock_journal_service_with_overrides.h"
#include "core/test/utils/conditional_wait.h"
#include "core/transaction_manager/mock/mock_transaction_command_serializer.h"
#include "core/transaction_manager/mock/mock_transaction_engine.h"
#include "core/transaction_manager/src/proto/transaction_engine.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RemoteTransactionManagerInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Transaction;
using google::scp::core::TransactionCommand;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::Serialization;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::test::WaitUntil;
using google::scp::core::transaction_manager::TransactionPhase;
using google::scp::core::transaction_manager::mock::
    MockTransactionCommandSerializer;
using google::scp::core::transaction_manager::mock::MockTransactionEngine;
using google::scp::core::transaction_manager::proto::TransactionCommandLog_1_0;
using google::scp::core::transaction_manager::proto::TransactionEngineLog;
using google::scp::core::transaction_manager::proto::TransactionEngineLog_1_0;
using google::scp::core::transaction_manager::proto::TransactionLog_1_0;
using google::scp::core::transaction_manager::proto::TransactionLogType;
using google::scp::core::transaction_manager::proto::TransactionPhaseLog_1_0;
using google::scp::cpio::MockMetricClient;
using std::atomic;
using std::function;
using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::thread;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::core::test {

TEST(TransactionEngineRemoteTest, VerifyExecuteRemoteOperation) {
  atomic<bool> condition = false;
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  shared_ptr<Transaction> current_transaction;
  TransactionPhase current_phase;
  mock_transaction_engine.log_transaction_and_proceed_to_next_phase_mock =
      [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
        current_phase = phase;
        current_transaction = transaction;
        condition = true;
        return SuccessExecutionResult();
      };

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  transaction_context.request->is_coordinated_remotely = true;
  transaction_context.request->transaction_secret =
      make_shared<string>("transaction_secret");
  transaction_context.request->transaction_origin =
      make_shared<string>("transaction_origin");

  mock_transaction_engine.Execute(transaction_context);
  WaitUntil([&condition]() { return condition.load(); });

  EXPECT_EQ(current_phase, TransactionPhase::NotStarted);
  EXPECT_NE(current_transaction, nullptr);
  EXPECT_EQ(current_transaction->id.low,
            transaction_context.request->transaction_id.low);
  EXPECT_EQ(current_transaction->id.high,
            transaction_context.request->transaction_id.high);
  EXPECT_EQ(current_transaction->current_phase, TransactionPhase::NotStarted);
  EXPECT_SUCCESS(current_transaction->current_phase_execution_result);
  EXPECT_EQ(current_transaction->pending_callbacks, 0);
  EXPECT_EQ(current_transaction->is_coordinated_remotely, true);

  shared_ptr<Transaction> stored_transaction;
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Find(
                current_transaction->id, stored_transaction),
            SuccessExecutionResult());

  EXPECT_EQ(current_transaction, stored_transaction);
}

TEST(TransactionEngineRemoteTest, ExecutePhaseNonmatchingTransactionFound) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();

  EXPECT_THAT(mock_transaction_engine.ExecutePhase(transaction_phase_context),
              ResultIs(FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND)));
}

TEST(TransactionEngineRemoteTest, ExecutePhaseRemoteAndWaitingCombinations) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  auto transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->is_coordinated_remotely = false;
  transaction->is_waiting_for_remote = true;
  transaction->transaction_secret = make_shared<string>("123");
  transaction->transaction_origin = make_shared<string>("123");

  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Begin;
  transaction_phase_context.request->transaction_secret =
      transaction->transaction_secret;
  transaction_phase_context.request->transaction_origin =
      transaction->transaction_origin;

  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY)));

  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;

  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_CURRENT_TRANSACTION_IS_RUNNING)));

  transaction->is_coordinated_remotely = false;
  transaction->is_waiting_for_remote = false;

  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY)));
}

void ExecuteNonPossiblePhases(vector<TransactionPhase> all_non_possible_phases,
                              TransactionExecutionPhase requested_phase) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();

  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;

  for (auto non_possible_phase : all_non_possible_phases) {
    MockTransactionEngine mock_transaction_engine(
        async_executor, mock_transaction_command_serializer,
        mock_journal_service, remote_transaction_manager, mock_metric_client);

    auto transaction_id = Uuid::GenerateUuid();
    auto transaction = make_shared<Transaction>();
    transaction->current_phase = non_possible_phase;
    transaction->is_coordinated_remotely = true;
    transaction->is_waiting_for_remote = true;
    transaction->transaction_secret = make_shared<string>("123");
    transaction->transaction_origin = make_shared<string>("1234");
    transaction->id = transaction_id;

    auto pair = make_pair(transaction_id, transaction);
    mock_transaction_engine.GetActiveTransactionsMap().Insert(pair,
                                                              transaction);

    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
        transaction_phase_context;
    transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
    transaction_phase_context.request->transaction_id = transaction_id;
    transaction_phase_context.request->transaction_execution_phase =
        requested_phase;
    transaction_phase_context.request->transaction_secret =
        transaction->transaction_secret;
    transaction_phase_context.request->transaction_origin =
        transaction->transaction_origin;

    EXPECT_THAT(mock_transaction_engine.ExecutePhase(transaction_phase_context),
                ResultIs(FailureExecutionResult(
                    errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE)));
  }
}

TEST(TransactionEngineRemoteTest,
     ExecutePhaseNonmatchingTransactionBeginPhase) {
  vector<TransactionPhase> all_non_possible_phases = {
      TransactionPhase::Unknown,     TransactionPhase::Prepare,
      TransactionPhase::Commit,      TransactionPhase::CommitNotify,
      TransactionPhase::AbortNotify, TransactionPhase::Committed,
      TransactionPhase::Aborted,     TransactionPhase::End,
  };

  ExecuteNonPossiblePhases(all_non_possible_phases,
                           TransactionExecutionPhase::Begin);
}

TEST(TransactionEngineRemoteTest,
     ExecutePhaseNonmatchingTransactionPreparePhase) {
  vector<TransactionPhase> all_non_possible_phases = {
      TransactionPhase::Unknown,     TransactionPhase::Begin,
      TransactionPhase::Commit,      TransactionPhase::CommitNotify,
      TransactionPhase::AbortNotify, TransactionPhase::Committed,
      TransactionPhase::Aborted,     TransactionPhase::End,
  };

  ExecuteNonPossiblePhases(all_non_possible_phases,
                           TransactionExecutionPhase::Prepare);
}

TEST(TransactionEngineRemoteTest,
     ExecutePhaseNonmatchingTransactionCommitPhase) {
  vector<TransactionPhase> all_non_possible_phases = {
      TransactionPhase::Unknown,     TransactionPhase::Prepare,
      TransactionPhase::Begin,       TransactionPhase::CommitNotify,
      TransactionPhase::AbortNotify, TransactionPhase::Committed,
      TransactionPhase::Aborted,     TransactionPhase::End,
  };

  ExecuteNonPossiblePhases(all_non_possible_phases,
                           TransactionExecutionPhase::Commit);
}

TEST(TransactionEngineRemoteTest,
     ExecutePhaseNonmatchingTransactionCommitNotifyPhase) {
  vector<TransactionPhase> all_non_possible_phases = {
      TransactionPhase::Unknown,     TransactionPhase::Prepare,
      TransactionPhase::Commit,      TransactionPhase::Begin,
      TransactionPhase::AbortNotify, TransactionPhase::Committed,
      TransactionPhase::Aborted,     TransactionPhase::End,
  };

  ExecuteNonPossiblePhases(all_non_possible_phases,
                           TransactionExecutionPhase::Notify);
}

TEST(TransactionEngineRemoteTest,
     ExecutePhaseNonmatchingTransactionAbortNotifyPhase) {
  vector<TransactionPhase> all_non_possible_phases = {
      TransactionPhase::Unknown,
      TransactionPhase::Committed,
      TransactionPhase::Aborted,
      TransactionPhase::End,
  };

  ExecuteNonPossiblePhases(all_non_possible_phases,
                           TransactionExecutionPhase::Abort);
}

TEST(TransactionEngineRemoteTest, ExecutePhaseNonmatchingTransactionEndPhase) {
  vector<TransactionPhase> all_non_possible_phases = {
      TransactionPhase::Unknown, TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::AbortNotify};

  ExecuteNonPossiblePhases(all_non_possible_phases,
                           TransactionExecutionPhase::End);
}

void ExecutePhaseProperCallbacksCalled(
    TransactionPhase transaction_phase,
    TransactionExecutionPhase requested_phase,
    function<void(MockTransactionEngine&)> mock_function) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();

  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  mock_function(mock_transaction_engine);

  auto transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = transaction_phase;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = true;
  transaction->last_execution_timestamp = 123456789;
  transaction->id = transaction_id;
  transaction->transaction_secret = make_shared<string>("secret");
  transaction->transaction_origin = make_shared<string>("origin");

  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->transaction_execution_phase =
      requested_phase;
  transaction_phase_context.request->last_execution_timestamp =
      transaction->last_execution_timestamp;
  transaction_phase_context.request->transaction_secret =
      transaction->transaction_secret;
  transaction_phase_context.request->transaction_origin =
      transaction->transaction_origin;

  EXPECT_SUCCESS(
      mock_transaction_engine.ExecutePhase(transaction_phase_context));
}

TEST(TransactionEngineRemoteTest, ExecutePhaseProperCallbacksCalledBegin) {
  atomic<bool> condition = false;
  function<void(MockTransactionEngine&)> mock =
      [&](MockTransactionEngine& transaction_engine) {
        transaction_engine.begin_transaction_mock =
            [&](shared_ptr<Transaction>& transaction) { condition = true; };
      };

  ExecutePhaseProperCallbacksCalled(TransactionPhase::Begin,
                                    TransactionExecutionPhase::Begin, mock);

  WaitUntil([&]() { return condition.load(); });
}

TEST(TransactionEngineRemoteTest, ExecutePhaseProperCallbacksCalledPrepare) {
  atomic<bool> condition = false;
  function<void(MockTransactionEngine&)> mock =
      [&](MockTransactionEngine& transaction_engine) {
        transaction_engine.prepare_transaction_mock =
            [&](shared_ptr<Transaction>& transaction) { condition = true; };
      };

  ExecutePhaseProperCallbacksCalled(TransactionPhase::Prepare,
                                    TransactionExecutionPhase::Prepare, mock);

  WaitUntil([&]() { return condition.load(); });
}

TEST(TransactionEngineRemoteTest, ExecutePhaseProperCallbacksCalledCommit) {
  atomic<bool> condition = false;
  function<void(MockTransactionEngine&)> mock =
      [&](MockTransactionEngine& transaction_engine) {
        transaction_engine.commit_transaction_mock =
            [&](shared_ptr<Transaction>& transaction) { condition = true; };
      };

  ExecutePhaseProperCallbacksCalled(TransactionPhase::Commit,
                                    TransactionExecutionPhase::Commit, mock);

  WaitUntil([&]() { return condition.load(); });
}

TEST(TransactionEngineRemoteTest,
     ExecutePhaseProperCallbacksCalledCommitNotify) {
  atomic<bool> condition = false;
  function<void(MockTransactionEngine&)> mock =
      [&](MockTransactionEngine& transaction_engine) {
        transaction_engine.commit_notify_transaction_mock =
            [&](shared_ptr<Transaction>& transaction) { condition = true; };
      };

  ExecutePhaseProperCallbacksCalled(TransactionPhase::CommitNotify,
                                    TransactionExecutionPhase::Notify, mock);

  WaitUntil([&]() { return condition.load(); });
}

TEST(TransactionEngineRemoteTest,
     ExecutePhaseProperCallbacksCalledAbortNotify) {
  atomic<bool> condition = false;
  function<void(MockTransactionEngine&)> mock =
      [&](MockTransactionEngine& transaction_engine) {
        transaction_engine.abort_notify_transaction_mock =
            [&](shared_ptr<Transaction>& transaction) {
              EXPECT_EQ(transaction->current_phase,
                        TransactionPhase::AbortNotify);
              condition = true;
            };
      };

  ExecutePhaseProperCallbacksCalled(TransactionPhase::AbortNotify,
                                    TransactionExecutionPhase::Abort, mock);

  WaitUntil([&]() { return condition.load(); });
}

TEST(TransactionEngineRemoteTest, ExecutePhaseProperCallbacksCalledEnd) {
  atomic<bool> condition = false;
  function<void(MockTransactionEngine&)> mock =
      [&](MockTransactionEngine& transaction_engine) {
        transaction_engine.end_transaction_mock =
            [&](shared_ptr<Transaction>& transaction) {
              EXPECT_EQ(transaction->current_phase, TransactionPhase::End);
              condition = true;
            };
      };

  ExecutePhaseProperCallbacksCalled(TransactionPhase::Aborted,
                                    TransactionExecutionPhase::End, mock);

  WaitUntil([&]() { return condition.load(); });

  condition = false;
  ExecutePhaseProperCallbacksCalled(TransactionPhase::Committed,
                                    TransactionExecutionPhase::End, mock);

  WaitUntil([&]() { return condition.load(); });

  condition = false;
  ExecutePhaseProperCallbacksCalled(TransactionPhase::End,
                                    TransactionExecutionPhase::End, mock);

  WaitUntil([&]() { return condition.load(); });
}

TEST(TransactionEngineRemoteTest, ProceedToNextPhaseRemotely) {
  atomic<bool> condition = false;
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  auto transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Begin;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_SUCCESS(transaction_phase_context.result);
            condition = true;
          });

  transaction->remote_phase_context = transaction_phase_context;

  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  mock_transaction_engine.ProceedToNextPhase(TransactionPhase::Begin,
                                             transaction);

  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(transaction->is_waiting_for_remote, true);
}

TEST(TransactionEngineRemoteTest, ProceedToNextPhaseRemotelyFailed) {
  atomic<bool> condition = false;
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  auto transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Begin;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->current_phase_execution_result = FailureExecutionResult(123);
  transaction->current_phase_failed = true;

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_THAT(transaction_phase_context.result,
                        ResultIs(FailureExecutionResult(123)));
            condition = true;
          });

  transaction->remote_phase_context = transaction_phase_context;

  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  mock_transaction_engine.ProceedToNextPhase(TransactionPhase::Begin,
                                             transaction);

  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(transaction->is_waiting_for_remote, true);
}

}  // namespace google::scp::core::test
