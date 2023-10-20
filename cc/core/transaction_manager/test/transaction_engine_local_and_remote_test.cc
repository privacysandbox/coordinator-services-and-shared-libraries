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

#include "core/async_executor/src/async_executor.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/journal_service/mock/mock_journal_service_with_overrides.h"
#include "core/test/utils/conditional_wait.h"
#include "core/transaction_manager/mock/mock_remote_transaction_manager.h"
#include "core/transaction_manager/mock/mock_transaction_command_serializer.h"
#include "core/transaction_manager/mock/mock_transaction_engine.h"
#include "core/transaction_manager/src/proto/transaction_engine.pb.h"
#include "core/transaction_manager/src/transaction_phase_manager.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RemoteTransactionManagerInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Transaction;
using google::scp::core::TransactionCommand;
using google::scp::core::TransactionPhaseManager;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::Serialization;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::test::WaitUntil;
using google::scp::core::transaction_manager::TransactionEngineInterface;
using google::scp::core::transaction_manager::TransactionPhase;
using google::scp::core::transaction_manager::TransactionPhaseManagerInterface;
using google::scp::core::transaction_manager::mock::
    MockRemoteTransactionManager;
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
using std::move;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::thread;
using std::vector;
using std::weak_ptr;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::core::test {

void CreateLocalRemoteTransactionManagers(
    shared_ptr<MockTransactionEngine>& mock_transaction_engine_1,
    shared_ptr<MockTransactionEngine>& mock_transaction_engine_2,
    function<void()>& init_function, function<void()>& run_function,
    function<void()>& stop_function) {
  shared_ptr<JournalServiceInterface> mock_journal_service_1 =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor_1 =
      make_shared<AsyncExecutor>(2, 1000);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer_1 =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager_1 =
      make_shared<TransactionPhaseManager>();
  auto mock_remote_transaction_manager_1 =
      make_shared<MockRemoteTransactionManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager_1 =
      mock_remote_transaction_manager_1;

  shared_ptr<JournalServiceInterface> mock_journal_service_2 =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor_2 =
      make_shared<AsyncExecutor>(2, 1000);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer_2 =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager_2 =
      make_shared<TransactionPhaseManager>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_remote_transaction_manager_2 =
      make_shared<MockRemoteTransactionManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager_2 =
      mock_remote_transaction_manager_2;

  mock_transaction_engine_1 = make_shared<MockTransactionEngine>(
      async_executor_1, mock_transaction_command_serializer_1,
      mock_journal_service_1, transaction_phase_manager_1,
      remote_transaction_manager_2, mock_metric_client,
      0 /* transaction_engine_cache_lifetime_seconds */);
  shared_ptr<TransactionEngineInterface> transaction_engine_1 =
      mock_transaction_engine_1;

  mock_transaction_engine_2 = make_shared<MockTransactionEngine>(
      async_executor_2, mock_transaction_command_serializer_2,
      mock_journal_service_2, transaction_phase_manager_2,
      remote_transaction_manager_1, mock_metric_client,
      0 /* transaction_engine_cache_lifetime_seconds */);
  shared_ptr<TransactionEngineInterface> transaction_engine_2 =
      mock_transaction_engine_2;

  mock_remote_transaction_manager_1->transaction_engine =
      weak_ptr<TransactionEngineInterface>(transaction_engine_1);
  mock_remote_transaction_manager_2->transaction_engine =
      weak_ptr<TransactionEngineInterface>(transaction_engine_2);

  init_function = [async_executor_1, async_executor_2, transaction_engine_1,
                   transaction_engine_2]() {
    EXPECT_SUCCESS(async_executor_1->Init());
    EXPECT_SUCCESS(async_executor_2->Init());
    EXPECT_SUCCESS(transaction_engine_1->Init());
    EXPECT_SUCCESS(transaction_engine_2->Init());
  };

  run_function = [async_executor_1, async_executor_2, transaction_engine_1,
                  transaction_engine_2]() {
    EXPECT_SUCCESS(async_executor_1->Run());
    EXPECT_SUCCESS(async_executor_2->Run());
    EXPECT_SUCCESS(transaction_engine_1->Run());
    EXPECT_SUCCESS(transaction_engine_2->Run());
  };

  stop_function = [async_executor_1, async_executor_2, transaction_engine_1,
                   transaction_engine_2]() {
    EXPECT_SUCCESS(transaction_engine_1->Stop());
    EXPECT_SUCCESS(async_executor_1->Stop());
    EXPECT_SUCCESS(transaction_engine_2->Stop());
    EXPECT_SUCCESS(async_executor_2->Stop());
  };
}

void CreateTransaction(
    Uuid& transaction_id, Timestamp expiration_time,
    AsyncContext<TransactionRequest, TransactionResponse>& transaction_context,
    shared_ptr<Transaction>& transaction) {
  transaction = make_shared<Transaction>();
  transaction->id = transaction_id;
  transaction->context = transaction_context;
  transaction->current_phase = TransactionPhase::NotStarted;
  transaction->transaction_execution_result =
      FailureExecutionResult(errors::SC_TRANSACTION_MANAGER_NOT_FINISHED);
  transaction->is_coordinated_remotely =
      transaction_context.request->is_coordinated_remotely;
  transaction->transaction_secret =
      transaction_context.request->transaction_secret;
  transaction->transaction_origin =
      transaction_context.request->transaction_origin;
  transaction->is_waiting_for_remote = false;
  transaction->current_phase_execution_result = SuccessExecutionResult();
  transaction->is_loaded = true;
  transaction->expiration_time = expiration_time;
}

TEST(TransactionEngineLocalAndRemoteTest,
     PendingCallbackShouldNotDeleteExpiredTransaction) {
  shared_ptr<MockTransactionEngine> mock_transaction_engine_1;
  shared_ptr<MockTransactionEngine> mock_transaction_engine_2;
  function<void()> init_function;
  function<void()> run_function;
  function<void()> stop_function;

  CreateLocalRemoteTransactionManagers(mock_transaction_engine_1,
                                       mock_transaction_engine_2, init_function,
                                       run_function, stop_function);
  init_function();
  run_function();

  // Both transactions are remotely coordinated transactions
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->is_coordinated_remotely = true;
  transaction_context.request->transaction_secret =
      make_shared<string>("this_is_a_transaction_secret");
  transaction_context.request->transaction_origin =
      make_shared<string>("origin.com");

  auto transaction_id = Uuid::GenerateUuid();
  shared_ptr<Transaction> transaction_local;
  CreateTransaction(transaction_id, UINT64_MAX, transaction_context,
                    transaction_local);
  shared_ptr<Transaction> transaction_remote;
  CreateTransaction(transaction_id, UINT64_MAX, transaction_context,
                    transaction_remote);

  transaction_local->pending_callbacks++;
  auto pair_local = make_pair(transaction_id, transaction_local);
  mock_transaction_engine_1->GetActiveTransactionsMap().Insert(
      pair_local, transaction_local);

  transaction_remote->pending_callbacks++;
  auto pair_remote = make_pair(transaction_id, transaction_remote);
  mock_transaction_engine_2->GetActiveTransactionsMap().Insert(
      pair_remote, transaction_remote);

  atomic<bool> transaction_1_called(false);
  mock_transaction_engine_1->on_before_garbage_collection_pre_caller =
      [&](Uuid&, shared_ptr<Transaction>&,
          function<void(bool)>& should_delete) {
        auto original_should_delete = should_delete;
        should_delete = [&, original_should_delete](bool delete_it) {
          EXPECT_EQ(delete_it, false);
          original_should_delete(delete_it);
          transaction_1_called = true;
        };
      };

  atomic<bool> transaction_2_called(false);
  mock_transaction_engine_2->on_before_garbage_collection_pre_caller =
      [&](Uuid&, shared_ptr<Transaction>&,
          function<void(bool)>& should_delete) {
        auto original_should_delete = should_delete;
        should_delete = [&, original_should_delete](bool delete_it) {
          EXPECT_EQ(delete_it, false);
          original_should_delete(delete_it);
          transaction_2_called = true;
        };
      };

  transaction_local->expiration_time = 0;
  transaction_remote->expiration_time = 0;

  // A remote transaction should be waiting for remote once it has done with
  // phase execution. stop_function() on Transaction Engine ensures that all
  // remote transactions are waiting for remote TM to issue commands to execute.
  transaction_remote->is_waiting_for_remote = true;
  transaction_local->is_waiting_for_remote = true;

  WaitUntil([&]() { return transaction_1_called && transaction_2_called; });
  stop_function();
}

void RunTransactionsWithDifferentSyncPhases(TransactionPhase local_phase,
                                            TransactionPhase remote_phase,
                                            bool local_phase_failed,
                                            bool remote_phase_failed) {
  shared_ptr<MockTransactionEngine> mock_transaction_engine_1;
  shared_ptr<MockTransactionEngine> mock_transaction_engine_2;
  function<void()> init_function;
  function<void()> run_function;
  function<void()> stop_function;

  CreateLocalRemoteTransactionManagers(mock_transaction_engine_1,
                                       mock_transaction_engine_2, init_function,
                                       run_function, stop_function);
  init_function();
  run_function();

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->is_coordinated_remotely = true;
  transaction_context.request->transaction_secret =
      make_shared<string>("this_is_a_transaction_secret");
  transaction_context.request->transaction_origin =
      make_shared<string>("origin.com");

  TransactionAction action = [](TransactionCommandCallback& callback) {
    auto result = SuccessExecutionResult();
    callback(result);
    return SuccessExecutionResult();
  };

  TransactionCommand command;
  command.begin = action;
  command.prepare = action;
  command.commit = action;
  command.notify = action;
  command.abort = action;
  command.end = action;

  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(move(command)));

  auto transaction_id = Uuid::GenerateUuid();
  shared_ptr<Transaction> transaction_local;
  CreateTransaction(transaction_id, UINT64_MAX, transaction_context,
                    transaction_local);
  transaction_local->current_phase = local_phase;
  transaction_local->is_waiting_for_remote = true;

  shared_ptr<Transaction> transaction_remote;
  CreateTransaction(transaction_id, UINT64_MAX, transaction_context,
                    transaction_remote);
  transaction_remote->current_phase = remote_phase;
  transaction_remote->is_waiting_for_remote = true;

  auto pair_local = make_pair(transaction_id, transaction_local);
  EXPECT_EQ(mock_transaction_engine_1->GetActiveTransactionsMap().Insert(
                pair_local, transaction_local),
            SuccessExecutionResult());

  auto pair_remote = make_pair(transaction_id, transaction_remote);
  EXPECT_EQ(mock_transaction_engine_2->GetActiveTransactionsMap().Insert(
                pair_remote, transaction_remote),
            SuccessExecutionResult());

  transaction_local->expiration_time = 0;
  transaction_remote->expiration_time = 0;

  WaitUntil([&]() {
    shared_ptr<Transaction> transaction;
    return !mock_transaction_engine_1->GetActiveTransactionsMap()
                .Find(transaction_id, transaction)
                .Successful() &&
           !mock_transaction_engine_2->GetActiveTransactionsMap()
                .Find(transaction_id, transaction)
                .Successful();
  });

  EXPECT_EQ(transaction_local->transaction_failed.load(), local_phase_failed);
  EXPECT_EQ(transaction_remote->transaction_failed.load(), remote_phase_failed);
  EXPECT_EQ(transaction_local->current_phase, TransactionPhase::End);
  EXPECT_EQ(transaction_remote->current_phase, TransactionPhase::End);
  stop_function();
}

void RunTransactionsWithDifferentSyncPhasesBackAndForth(
    TransactionPhase local_phase, TransactionPhase remote_phase,
    bool local_phase_failed = true, bool remote_phase_failed = true) {
  RunTransactionsWithDifferentSyncPhases(
      local_phase, remote_phase, local_phase_failed, remote_phase_failed);
  RunTransactionsWithDifferentSyncPhases(
      remote_phase, local_phase, remote_phase_failed, local_phase_failed);
}

TEST(TransactionEngineLocalAndRemoteTest, Begin) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(TransactionPhase::Begin,
                                                     TransactionPhase::Begin);
}

TEST(TransactionEngineLocalAndRemoteTest, BeginAndPrepare) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(TransactionPhase::Begin,
                                                     TransactionPhase::Prepare);
}

TEST(TransactionEngineLocalAndRemoteTest, PrepareAndCommit) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(TransactionPhase::Prepare,
                                                     TransactionPhase::Commit);
}

TEST(TransactionEngineLocalAndRemoteTest, Commit) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(TransactionPhase::Commit,
                                                     TransactionPhase::Commit);
}

TEST(TransactionEngineLocalAndRemoteTest, CommitAndCommitNotify) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(
      TransactionPhase::Commit, TransactionPhase::CommitNotify, true, true);
}

TEST(TransactionEngineLocalAndRemoteTest, CommitAndAbortNotify) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(
      TransactionPhase::Commit, TransactionPhase::AbortNotify, true, true);
}

TEST(TransactionEngineLocalAndRemoteTest, CommitNotify) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(
      TransactionPhase::CommitNotify, TransactionPhase::CommitNotify, true,
      true);
}

TEST(TransactionEngineLocalAndRemoteTest, CommitNotifyAndAbortNotify) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(
      TransactionPhase::CommitNotify, TransactionPhase::AbortNotify, true,
      true);
}

TEST(TransactionEngineLocalAndRemoteTest, AbortNotify) {
  RunTransactionsWithDifferentSyncPhasesBackAndForth(
      TransactionPhase::AbortNotify, TransactionPhase::AbortNotify, true, true);
}

void RunTransactionsWithDifferentOutOfSyncPhases(
    TransactionPhase local_phase, TransactionPhase remote_phase) {
  shared_ptr<MockTransactionEngine> mock_transaction_engine_1;
  shared_ptr<MockTransactionEngine> mock_transaction_engine_2;
  function<void()> init_function;
  function<void()> run_function;
  function<void()> stop_function;

  CreateLocalRemoteTransactionManagers(mock_transaction_engine_1,
                                       mock_transaction_engine_2, init_function,
                                       run_function, stop_function);
  init_function();
  run_function();

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->is_coordinated_remotely = true;
  transaction_context.request->transaction_secret =
      make_shared<string>("this_is_a_transaction_secret");
  transaction_context.request->transaction_origin =
      make_shared<string>("origin.com");

  TransactionAction action = [](TransactionCommandCallback& callback) {
    auto result = SuccessExecutionResult();
    callback(result);
    return SuccessExecutionResult();
  };

  TransactionCommand command;
  command.prepare = action;
  command.commit = action;
  command.notify = action;
  command.abort = action;
  command.end = action;

  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(move(command)));

  auto transaction_id = Uuid::GenerateUuid();
  shared_ptr<Transaction> transaction_local;
  CreateTransaction(transaction_id, UINT64_MAX, transaction_context,
                    transaction_local);
  transaction_local->current_phase = local_phase;
  transaction_local->is_waiting_for_remote = true;

  shared_ptr<Transaction> transaction_remote;
  CreateTransaction(transaction_id, UINT64_MAX, transaction_context,
                    transaction_remote);
  transaction_remote->current_phase = remote_phase;
  transaction_remote->is_waiting_for_remote = true;

  auto pair_local = make_pair(transaction_id, transaction_local);
  EXPECT_EQ(mock_transaction_engine_1->GetActiveTransactionsMap().Insert(
                pair_local, transaction_local),
            SuccessExecutionResult());

  auto pair_remote = make_pair(transaction_id, transaction_remote);
  EXPECT_EQ(mock_transaction_engine_2->GetActiveTransactionsMap().Insert(
                pair_remote, transaction_remote),
            SuccessExecutionResult());

  transaction_local->expiration_time = 0;
  transaction_remote->expiration_time = 0;
  stop_function();
}

void RunTransactionsWithDifferentOutOfSyncPhasesBackAndForth(
    TransactionPhase local_phase, TransactionPhase remote_phase) {
  RunTransactionsWithDifferentOutOfSyncPhases(local_phase, remote_phase);
  RunTransactionsWithDifferentOutOfSyncPhases(remote_phase, local_phase);
}

TEST(TransactionEngineLocalAndRemoteTest, BeginOutOfSync) {
  vector<TransactionPhase> out_of_sync_phases = {
      TransactionPhase::Commit, TransactionPhase::CommitNotify,
      TransactionPhase::Committed, TransactionPhase::Aborted,
      TransactionPhase::End};

  for (auto phase : out_of_sync_phases) {
    RunTransactionsWithDifferentOutOfSyncPhasesBackAndForth(
        TransactionPhase::Begin, phase);
  }
}

TEST(TransactionEngineLocalAndRemoteTest, PrepareOutOfSync) {
  vector<TransactionPhase> out_of_sync_phases = {
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::Aborted, TransactionPhase::End};

  for (auto phase : out_of_sync_phases) {
    RunTransactionsWithDifferentOutOfSyncPhasesBackAndForth(
        TransactionPhase::Prepare, phase);
  }
}

TEST(TransactionEngineLocalAndRemoteTest, CommitOutOfSync) {
  vector<TransactionPhase> out_of_sync_phases = {
      TransactionPhase::Begin, TransactionPhase::Committed,
      TransactionPhase::Aborted, TransactionPhase::End};

  for (auto phase : out_of_sync_phases) {
    RunTransactionsWithDifferentOutOfSyncPhasesBackAndForth(
        TransactionPhase::Commit, phase);
  }
}

TEST(TransactionEngineLocalAndRemoteTest, CommitNotifyOutOfSync) {
  vector<TransactionPhase> out_of_sync_phases = {TransactionPhase::Begin,
                                                 TransactionPhase::Prepare};

  for (auto phase : out_of_sync_phases) {
    RunTransactionsWithDifferentOutOfSyncPhasesBackAndForth(
        TransactionPhase::CommitNotify, phase);
  }
}

TEST(TransactionEngineLocalAndRemoteTest, AbortedOutOfSync) {
  vector<TransactionPhase> out_of_sync_phases = {
      TransactionPhase::Begin,
      TransactionPhase::Prepare,
      TransactionPhase::Commit,
  };

  for (auto phase : out_of_sync_phases) {
    RunTransactionsWithDifferentOutOfSyncPhasesBackAndForth(
        TransactionPhase::Aborted, phase);
  }
}

TEST(TransactionEngineLocalAndRemoteTest, CommittedOutOfSync) {
  vector<TransactionPhase> out_of_sync_phases = {
      TransactionPhase::Begin,
      TransactionPhase::Prepare,
      TransactionPhase::Commit,
  };

  for (auto phase : out_of_sync_phases) {
    RunTransactionsWithDifferentOutOfSyncPhasesBackAndForth(
        TransactionPhase::Committed, phase);
  }
}
}  // namespace google::scp::core::test
