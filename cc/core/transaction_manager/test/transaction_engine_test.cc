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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/mock/mock_async_executor_with_internals.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/proto/common.pb.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/logger_interface.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/journal_service/mock/mock_journal_service_with_overrides.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "core/transaction_manager/mock/mock_remote_transaction_manager.h"
#include "core/transaction_manager/mock/mock_transaction_command_serializer.h"
#include "core/transaction_manager/mock/mock_transaction_engine.h"
#include "core/transaction_manager/src/proto/transaction_engine.pb.h"
#include "core/transaction_manager/src/transaction_phase_manager.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::CheckpointLog;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LoggerInterface;
using google::scp::core::RemoteTransactionManagerInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Transaction;
using google::scp::core::TransactionCommand;
using google::scp::core::TransactionEngine;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::async_executor::mock::MockAsyncExecutorWithInternals;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::Serialization;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::journal_service::mock::MockJournalServiceWithOverrides;
using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::Logger;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
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
using std::dynamic_pointer_cast;
using std::function;
using std::list;
using std::make_pair;
using std::make_shared;
using std::map;
using std::set;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::thread;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

static constexpr Uuid kDefaultUuid = {0, 0};

namespace google::scp::core::test {

class TransactionEngineTest : public testing::Test {
 protected:
  static void SetUpTestSuite() { TestLoggingUtils::EnableLogOutputToConsole(); }

  void CreateComponents() {
    mock_journal_service_ = static_pointer_cast<JournalServiceInterface>(
        make_shared<MockJournalService>());
    mock_transaction_command_serializer_ =
        make_shared<MockTransactionCommandSerializer>();
    mock_async_executor_ = make_shared<MockAsyncExecutor>();
    async_executor_ = mock_async_executor_;
    auto mock_metric_client = make_shared<MockMetricClient>();
    mock_transaction_engine_ = make_shared<MockTransactionEngine>(
        async_executor_, mock_transaction_command_serializer_,
        mock_journal_service_, remote_transaction_manager_, mock_metric_client);
  }

  TransactionEngineTest() { CreateComponents(); }

  shared_ptr<BytesBuffer> GetSampleTransactionPhaseLogBytes(
      Uuid transaction_id, transaction_manager::proto::TransactionPhase phase,
      bool is_failed = true,
      common::proto::ExecutionStatus status =
          common::proto::ExecutionStatus::EXECUTION_STATUS_FAILURE,
      uint64_t status_code = 1234);

  shared_ptr<JournalServiceInterface> mock_journal_service_;
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<MockAsyncExecutor> mock_async_executor_;
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager_;
  shared_ptr<MockTransactionEngine> mock_transaction_engine_;
};

shared_ptr<BytesBuffer>
TransactionEngineTest::GetSampleTransactionPhaseLogBytes(
    Uuid transaction_id, transaction_manager::proto::TransactionPhase phase,
    bool is_failed, common::proto::ExecutionStatus status,
    uint64_t status_code) {
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(
      TransactionLogType::TRANSACTION_PHASE_LOG);

  TransactionPhaseLog_1_0 transaction_phase_log_1_0;
  transaction_phase_log_1_0.mutable_id()->set_high(transaction_id.high);
  transaction_phase_log_1_0.mutable_id()->set_low(transaction_id.low);
  transaction_phase_log_1_0.set_phase(phase);
  transaction_phase_log_1_0.set_failed(is_failed);
  transaction_phase_log_1_0.mutable_result()->set_status(status);
  transaction_phase_log_1_0.mutable_result()->set_status_code(status_code);

  BytesBuffer transaction_phase_log_1_0_bytes_buffer(
      transaction_phase_log_1_0.ByteSizeLong());
  transaction_phase_log_1_0.SerializeToArray(
      transaction_phase_log_1_0_bytes_buffer.bytes->data(),
      transaction_phase_log_1_0_bytes_buffer.capacity);
  transaction_phase_log_1_0_bytes_buffer.length =
      transaction_phase_log_1_0.ByteSizeLong();

  transaction_engine_log_1_0.set_log_body(
      transaction_phase_log_1_0_bytes_buffer.bytes->data(),
      transaction_phase_log_1_0_bytes_buffer.length);

  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());
  transaction_engine_log_1_0.SerializeToArray(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.capacity);
  transaction_engine_log_1_0_bytes_buffer.length =
      transaction_engine_log_1_0.ByteSizeLong();

  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  return bytes_buffer;
}

TEST_F(TransactionEngineTest, InitShouldSubscribe) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalServiceWithOverrides>(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  EXPECT_SUCCESS(mock_transaction_engine.Init());
  Uuid transaction_engine_uuid = {.high = 0xFFFFFFF1, .low = 0x00000004};
  OnLogRecoveredCallback callback;
  EXPECT_EQ(mock_journal_service->GetSubscribersMap().Find(
                transaction_engine_uuid, callback),
            SuccessExecutionResult());
}

TEST_F(TransactionEngineTest, RunShouldReplayAllPendingTransactions) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->context.request = make_shared<TransactionRequest>();
  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  transaction_id = Uuid::GenerateUuid();
  transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::End;
  pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  transaction_id = Uuid::GenerateUuid();
  transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Prepare;
  transaction->context.request = make_shared<TransactionRequest>();
  pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  auto remote_phase_called = false;
  transaction_id = Uuid::GenerateUuid();
  auto remote_transaction = make_shared<Transaction>();
  remote_transaction->current_phase = TransactionPhase::Prepare;
  remote_transaction->is_coordinated_remotely = true;
  remote_transaction->context.request = make_shared<TransactionRequest>();
  remote_transaction->remote_phase_context.callback = [&](auto&) {
    remote_phase_called = true;
  };
  pair = make_pair(transaction_id, remote_transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair,
                                                            remote_transaction);
  mock_transaction_engine.prepare_transaction_mock =
      [](shared_ptr<Transaction>& transaction) {
        AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse> context(
            make_shared<TransactionPhaseRequest>(),
            [](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                   context) {});
        transaction->remote_phase_context.callback(context);
      };

  // Total calls must become 2
  // 1 for recovery
  size_t total_calls = 0;
  mock_async_executor->schedule_mock = [&](const auto& work) {
    total_calls++;
    work();
    return SuccessExecutionResult();
  };
  mock_async_executor->schedule_for_mock = [&](const auto& a, auto timestmap,
                                               auto& cancellation_callback) {
    cancellation_callback = []() { return true; };
    return SuccessExecutionResult();
  };
  EXPECT_SUCCESS(mock_async_executor->Init());
  EXPECT_SUCCESS(mock_async_executor->Run());

  EXPECT_SUCCESS(mock_transaction_engine.Run());
  WaitUntil([&]() { return total_calls == 3; });
  EXPECT_EQ(remote_phase_called, true);

  // A remote transaction should be waiting for remote once it has done with
  // phase execution. Stop() on Transaction Engine ensures that all remote
  // transactions are waiting for remote TM to issue commands to execute.
  remote_transaction->is_waiting_for_remote = true;

  EXPECT_SUCCESS(mock_transaction_engine.Stop());
  EXPECT_SUCCESS(mock_async_executor->Stop());
}

TEST_F(TransactionEngineTest, VerifyExecuteOperationInvalidInitialization) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(123)};

  for (auto result : results) {
    shared_ptr<Transaction> current_transaction;
    mock_transaction_engine.initialize_transaction_mock =
        [&](AsyncContext<TransactionRequest, TransactionResponse>&,
            shared_ptr<Transaction>& transaction) { return result; };

    AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
    transaction_context.request = make_shared<TransactionRequest>();
    transaction_context.request->transaction_id = Uuid::GenerateUuid();

    EXPECT_THAT(mock_transaction_engine.Execute(transaction_context),
                ResultIs(result));
  }

  mock_transaction_engine.initialize_transaction_mock =
      [&](AsyncContext<TransactionRequest, TransactionResponse>&,
          shared_ptr<Transaction>& transaction) {
        return SuccessExecutionResult();
      };

  for (auto result : results) {
    shared_ptr<Transaction> current_transaction;
    mock_transaction_engine.log_transaction_and_proceed_to_next_phase_mock =
        [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
          return result;
        };

    AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
    transaction_context.request = make_shared<TransactionRequest>();
    transaction_context.request->transaction_id = Uuid::GenerateUuid();

    EXPECT_THAT(mock_transaction_engine.Execute(transaction_context),
                ResultIs(result));
  }
}

TEST_F(TransactionEngineTest, VerifyExecuteOperation) {
  atomic<bool> condition = false;
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
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
  EXPECT_EQ(current_transaction->is_coordinated_remotely, false);
  EXPECT_EQ(current_transaction->is_waiting_for_remote, false);

  shared_ptr<Transaction> stored_transaction;
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Find(
                current_transaction->id, stored_transaction),
            SuccessExecutionResult());

  EXPECT_EQ(current_transaction, stored_transaction);
}

TEST_F(TransactionEngineTest, VerifyNotStartedOperation) {
  atomic<bool> condition = false;
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<Transaction> current_transaction;

  mock_transaction_engine.log_transaction_and_proceed_to_next_phase_mock =
      [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ProceedToNextPhase(phase, transaction);
        return SuccessExecutionResult();
      };

  mock_transaction_engine.begin_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        current_transaction = transaction;
        condition = true;
      };

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();

  mock_transaction_engine.Execute(transaction_context);
  WaitUntil([&condition]() { return condition.load(); });

  EXPECT_EQ(current_transaction->current_phase, TransactionPhase::Begin);
  EXPECT_SUCCESS(current_transaction->current_phase_execution_result);
  EXPECT_EQ(current_transaction->pending_callbacks, 0);
}

TEST_F(TransactionEngineTest, VerifyBeginOperation) {
  atomic<bool> condition = false;
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<Transaction> current_transaction;

  mock_transaction_engine.log_transaction_and_proceed_to_next_phase_mock =
      [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ProceedToNextPhase(phase, transaction);
        return SuccessExecutionResult();
      };

  mock_transaction_engine.begin_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        current_transaction = transaction;
        condition = true;
      };

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();

  mock_transaction_engine.Execute(transaction_context);
  WaitUntil([&condition]() { return condition.load(); });

  EXPECT_EQ(current_transaction->current_phase, TransactionPhase::Begin);
  EXPECT_SUCCESS(current_transaction->current_phase_execution_result);
  EXPECT_EQ(current_transaction->pending_callbacks, 0);
}

TEST_F(TransactionEngineTest, VerifyPrepareOperation) {
  atomic<bool> condition = false;
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<Transaction> current_transaction;

  mock_transaction_engine.log_transaction_and_proceed_to_next_phase_mock =
      [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ProceedToNextPhase(phase, transaction);
        return SuccessExecutionResult();
      };

  mock_transaction_engine.begin_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ProceedToNextPhase(TransactionPhase::Begin,
                                                   transaction);
      };

  mock_transaction_engine.execute_distributed_phase_mock =
      [&](TransactionPhase transaction_phase,
          shared_ptr<Transaction>& transaction) mutable {
        EXPECT_EQ(transaction_phase, TransactionPhase::Prepare);
        current_transaction = transaction;
        condition = true;
      };

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  TransactionCommand command;
  command.prepare = [](TransactionCommandCallback&) {
    return SuccessExecutionResult();
  };
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(command));
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(command));
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(command));
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(command));

  mock_transaction_engine.Execute(transaction_context);
  WaitUntil([&condition]() { return condition.load(); });

  EXPECT_EQ(current_transaction->current_phase, TransactionPhase::Prepare);
  EXPECT_SUCCESS(current_transaction->current_phase_execution_result);
  EXPECT_EQ(current_transaction->pending_callbacks, 0);
}

void VerifyDispatchedOperations(
    TransactionPhase previous_phase, TransactionPhase current_phase,
    TransactionCommand transaction_command,
    function<void(MockTransactionEngine& mock_transaction_engine,
                  shared_ptr<Transaction>& current_transaction,
                  atomic<bool>& condition)>
        mock_function,
    ExecutionResult transaction_execution_result, size_t pending_callbacks) {
  atomic<bool> condition = false;
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<Transaction> current_transaction;
  mock_function(mock_transaction_engine, current_transaction, condition);

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(transaction_command));
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(transaction_command));
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(transaction_command));
  transaction_context.request->commands.push_back(
      make_shared<TransactionCommand>(transaction_command));

  mock_transaction_engine.Execute(transaction_context);
  WaitUntil([&condition]() { return condition.load(); });

  EXPECT_EQ(current_transaction->current_phase, current_phase);
  EXPECT_EQ(current_transaction->current_phase_execution_result,
            transaction_execution_result);
  EXPECT_EQ(current_transaction->pending_callbacks, pending_callbacks);
}

TEST_F(TransactionEngineTest, VerifyDispatchedOperations) {
  vector<TransactionPhase> current_phases = {TransactionPhase::Prepare,
                                             TransactionPhase::Commit,
                                             TransactionPhase::CommitNotify};

  vector<TransactionPhase> previous_phases = {TransactionPhase::Begin,
                                              TransactionPhase::Prepare,
                                              TransactionPhase::Commit};

  TransactionAction action = [](TransactionCommandCallback&) {
    return SuccessExecutionResult();
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    atomic<size_t> total_dispatched_commands = 0;
    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                mock_transaction_engine.dispatch_distributed_command_mock =
                    [&](TransactionPhase transaction_phase,
                        shared_ptr<TransactionCommand>& transaction_command,
                        shared_ptr<Transaction>& transaction) mutable {
                      if (total_dispatched_commands.fetch_add(1) == 3) {
                        current_transaction = transaction;
                        condition = true;
                      }
                      return SuccessExecutionResult();
                    };
                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, SuccessExecutionResult(), 4 /* pending_callbacks */);
  }
}

TEST_F(TransactionEngineTest, VerifyDispatchedOperationsFailedPartially) {
  vector<TransactionPhase> current_phases = {
      TransactionPhase::Prepare, TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::AbortNotify};

  vector<TransactionPhase> previous_phases = {
      TransactionPhase::Begin, TransactionPhase::Prepare,
      TransactionPhase::Commit, TransactionPhase::Commit};

  TransactionAction action = [](TransactionCommandCallback&) {
    return SuccessExecutionResult();
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    atomic<size_t> total_dispatched_commands = 0;
    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                mock_transaction_engine.dispatch_distributed_command_mock =
                    [&](TransactionPhase transaction_phase,
                        shared_ptr<TransactionCommand>& transaction_command,
                        shared_ptr<Transaction>& transaction) mutable {
                      auto prev_value = total_dispatched_commands.fetch_add(1);
                      ExecutionResult result = SuccessExecutionResult();
                      if (prev_value % 2 == 0) {
                        result = FailureExecutionResult(1);
                      }

                      if (prev_value == 3) {
                        current_transaction = transaction;
                        condition = true;
                      }

                      return result;
                    };

                if (i == 3) {
                  transaction->current_phase_execution_result =
                      FailureExecutionResult(1);
                }

                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, FailureExecutionResult(1), 2 /* pending_callbacks */);
  }
}

TEST_F(TransactionEngineTest, VerifyDispatchedOperationsFailedCompletely) {
  vector<TransactionPhase> current_phases = {
      TransactionPhase::Prepare, TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::AbortNotify};

  vector<TransactionPhase> previous_phases = {
      TransactionPhase::Begin, TransactionPhase::Prepare,
      TransactionPhase::Commit, TransactionPhase::Commit};

  TransactionAction action = [](TransactionCommandCallback&) {
    return SuccessExecutionResult();
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    atomic<size_t> total_dispatched_commands = 0;
    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                mock_transaction_engine.dispatch_distributed_command_mock =
                    [&](TransactionPhase transaction_phase,
                        shared_ptr<TransactionCommand>& transaction_command,
                        shared_ptr<Transaction>& transaction) mutable {
                      if (total_dispatched_commands.fetch_add(1) == 3) {
                        mock_transaction_engine.proceed_to_next_phase_mock =
                            [&](TransactionPhase transaction_phase,
                                shared_ptr<Transaction>& transaction) {
                              EXPECT_EQ(transaction_phase, current_phases[i]);
                              current_transaction = transaction;
                              condition = true;
                            };
                      }
                      return FailureExecutionResult(1);
                    };

                if (i == 3) {
                  transaction->current_phase_execution_result =
                      FailureExecutionResult(1);
                }

                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, FailureExecutionResult(1), 0 /* pending_callbacks */);
  }
}

TEST_F(TransactionEngineTest, VerifyDispatchedOperationsNotified) {
  vector<TransactionPhase> current_phases = {TransactionPhase::Prepare,
                                             TransactionPhase::Commit,
                                             TransactionPhase::CommitNotify};

  vector<TransactionPhase> previous_phases = {TransactionPhase::Begin,
                                              TransactionPhase::Prepare,
                                              TransactionPhase::Commit};

  TransactionAction action = [](TransactionCommandCallback& callback) {
    auto execution_result = SuccessExecutionResult();
    callback(execution_result);
    return execution_result;
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    atomic<size_t> total_callbacks = 0;
    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                mock_transaction_engine.on_phase_callback_mock =
                    [&](TransactionPhase transaction_phase,
                        ExecutionResult& execution_result,
                        shared_ptr<Transaction>& transaction) {
                      EXPECT_SUCCESS(execution_result);
                      EXPECT_EQ(transaction_phase, current_phases[i]);

                      if (total_callbacks.fetch_add(1) == 3) {
                        current_transaction = transaction;
                        condition = true;
                      }
                    };
                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, SuccessExecutionResult(), 4 /* pending_callbacks */);
  }
}

TEST_F(TransactionEngineTest, VerifyDispatchedOperationsNotifiedWithFailure) {
  vector<TransactionPhase> current_phases = {
      TransactionPhase::Aborted, TransactionPhase::AbortNotify,
      TransactionPhase::Unknown, TransactionPhase::AbortNotify};

  vector<TransactionPhase> previous_phases = {
      TransactionPhase::Begin, TransactionPhase::Prepare,
      TransactionPhase::Commit, TransactionPhase::Commit};

  atomic<size_t> total_callbacks = 0;
  TransactionAction action = [&](TransactionCommandCallback& callback) {
    ExecutionResult execution_result = SuccessExecutionResult();
    if (total_callbacks++ == 3) {
      execution_result = FailureExecutionResult(1);
    } else {
      callback(execution_result);
    }

    return execution_result;
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    total_callbacks = 0;
    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                if (previous_phases[i] == TransactionPhase::Begin) {
                  mock_transaction_engine.aborted_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::Prepare) {
                  mock_transaction_engine.abort_notify_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::Commit) {
                  mock_transaction_engine.unknown_state_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (i == 3) {
                  mock_transaction_engine.abort_notify_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                  transaction->current_phase_failed = true;
                  transaction->current_phase_execution_result =
                      FailureExecutionResult(1);
                }

                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, SuccessExecutionResult(), 0 /* pending_callbacks */);
  }
}

TEST_F(TransactionEngineTest, VerifyDispatchedOperationsNotifiedWithSuccess) {
  vector<TransactionPhase> current_phases = {
      TransactionPhase::Commit, TransactionPhase::CommitNotify,
      TransactionPhase::Committed, TransactionPhase::Aborted};

  vector<TransactionPhase> previous_phases = {
      TransactionPhase::Prepare, TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::AbortNotify};

  atomic<size_t> total_callbacks = 0;
  TransactionAction action = [&](TransactionCommandCallback& callback) {
    ExecutionResult execution_result = SuccessExecutionResult();
    callback(execution_result);
    return execution_result;
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    total_callbacks = 0;
    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                if (previous_phases[i] == TransactionPhase::Prepare) {
                  mock_transaction_engine.commit_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::Commit) {
                  mock_transaction_engine.commit_notify_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::CommitNotify) {
                  mock_transaction_engine.committed_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::AbortNotify) {
                  mock_transaction_engine.aborted_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, SuccessExecutionResult(), 0 /* pending_callbacks */);
  }
}

TEST_F(TransactionEngineTest,
       VerifyDispatchedOperationsNotifiedWithFailureMultiThreaded) {
  vector<TransactionPhase> current_phases = {
      TransactionPhase::Aborted, TransactionPhase::AbortNotify,
      TransactionPhase::Unknown, TransactionPhase::AbortNotify};

  vector<TransactionPhase> previous_phases = {
      TransactionPhase::Begin, TransactionPhase::Prepare,
      TransactionPhase::Commit, TransactionPhase::Commit};

  atomic<size_t> total_callbacks = 0;
  vector<thread> threads;
  TransactionAction action = [&](TransactionCommandCallback& callback) {
    ExecutionResult execution_result = SuccessExecutionResult();
    if (total_callbacks++ == 3) {
      execution_result = FailureExecutionResult(1);
    } else {
      threads.push_back(thread([callback, execution_result]() mutable {
        sleep_for(milliseconds(200));
        callback(execution_result);
      }));
    }

    return execution_result;
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    total_callbacks = 0;
    threads.clear();

    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                if (previous_phases[i] == TransactionPhase::Begin) {
                  mock_transaction_engine.aborted_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::Prepare) {
                  mock_transaction_engine.abort_notify_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::Commit) {
                  mock_transaction_engine.unknown_state_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (i == 3) {
                  mock_transaction_engine.abort_notify_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                  transaction->current_phase_failed = true;
                  transaction->current_phase_execution_result =
                      FailureExecutionResult(1);
                }

                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, SuccessExecutionResult(), 0 /* pending_callbacks */);

    for (auto& thread : threads) {
      thread.join();
    }
  }
}

TEST_F(TransactionEngineTest,
       VerifyDispatchedOperationsNotifiedWithSuccessMultiThreaded) {
  vector<TransactionPhase> current_phases = {
      TransactionPhase::Commit, TransactionPhase::CommitNotify,
      TransactionPhase::Committed, TransactionPhase::Aborted};

  vector<TransactionPhase> previous_phases = {
      TransactionPhase::Prepare, TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::AbortNotify};

  atomic<size_t> total_callbacks = 0;
  vector<thread> threads;
  TransactionAction action = [&](TransactionCommandCallback& callback) {
    ExecutionResult execution_result = SuccessExecutionResult();
    threads.push_back(thread([callback, execution_result]() mutable {
      sleep_for(milliseconds(200));
      callback(execution_result);
    }));
    return execution_result;
  };

  vector<TransactionCommand> transaction_commands;
  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().prepare = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().commit = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  transaction_commands.push_back(TransactionCommand());
  transaction_commands.back().notify = action;

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    total_callbacks = 0;
    threads.clear();
    function<void(MockTransactionEngine & mock_transaction_engine,
                  shared_ptr<Transaction> & current_transaction,
                  atomic<bool> & condition)>
        mock_function = [&](MockTransactionEngine& mock_transaction_engine,
                            shared_ptr<Transaction>& current_transaction,
                            atomic<bool>& condition) mutable {
          mock_transaction_engine
              .log_transaction_and_proceed_to_next_phase_mock =
              [&](TransactionPhase phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ProceedToNextPhase(phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
              [&](TransactionPhase transaction_phase,
                  shared_ptr<Transaction>& transaction) {
                mock_transaction_engine.ExecuteDistributedPhase(
                    transaction_phase, transaction);
                return SuccessExecutionResult();
              };

          mock_transaction_engine.begin_transaction_mock =
              [&](shared_ptr<Transaction>& transaction) mutable {
                if (previous_phases[i] == TransactionPhase::Prepare) {
                  mock_transaction_engine.commit_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::Commit) {
                  mock_transaction_engine.commit_notify_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::CommitNotify) {
                  mock_transaction_engine.committed_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                if (previous_phases[i] == TransactionPhase::AbortNotify) {
                  mock_transaction_engine.aborted_transaction_mock =
                      [&](shared_ptr<Transaction>& transaction) {
                        current_transaction = transaction;
                        condition = true;
                      };
                }

                transaction->current_phase = previous_phases[i];
                mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                           transaction);
              };
        };

    VerifyDispatchedOperations(
        previous_phases[i], current_phases[i], transaction_commands[i],
        mock_function, SuccessExecutionResult(), 0 /* pending_callbacks */);

    for (auto& thread : threads) {
      thread.join();
    }
  }
}

TEST_F(TransactionEngineTest, VerifyNonDispatchedSuccessNextPhases) {
  vector<TransactionPhase> current_phases = {
      TransactionPhase::Prepare, TransactionPhase::End, TransactionPhase::End};

  vector<TransactionPhase> previous_phases = {TransactionPhase::Begin,
                                              TransactionPhase::Committed,
                                              TransactionPhase::Aborted};

  auto mock_metric_client = make_shared<MockMetricClient>();
  atomic<bool> condition = false;
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<Transaction> current_transaction;

  mock_transaction_engine.log_transaction_and_proceed_to_next_phase_mock =
      [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ProceedToNextPhase(phase, transaction);
        return SuccessExecutionResult();
      };

  mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
      [&](TransactionPhase transaction_phase,
          shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ExecuteDistributedPhase(transaction_phase,
                                                        transaction);
        return SuccessExecutionResult();
      };

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    mock_transaction_engine.begin_transaction_mock =
        [&](shared_ptr<Transaction>& transaction) mutable {
          if (previous_phases[i] == TransactionPhase::Begin) {
            mock_transaction_engine.prepare_transaction_mock =
                [&](shared_ptr<Transaction>& transaction) {
                  current_transaction = transaction;
                  condition = true;
                };
          }

          if (previous_phases[i] == TransactionPhase::Committed) {
            mock_transaction_engine.end_transaction_mock =
                [&](shared_ptr<Transaction>& transaction) {
                  current_transaction = transaction;
                  condition = true;
                };
          }

          if (previous_phases[i] == TransactionPhase::Aborted) {
            mock_transaction_engine.end_transaction_mock =
                [&](shared_ptr<Transaction>& transaction) {
                  current_transaction = transaction;
                  condition = true;
                };
          }

          transaction->current_phase = previous_phases[i];
          mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                     transaction);
        };

    AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
    transaction_context.request = make_shared<TransactionRequest>();
    transaction_context.request->transaction_id = Uuid::GenerateUuid();
    mock_transaction_engine.Execute(transaction_context);
    WaitUntil([&condition]() { return condition.load(); });

    EXPECT_EQ(current_transaction->current_phase, current_phases[i]);
    EXPECT_SUCCESS(current_transaction->current_phase_execution_result);
    EXPECT_EQ(current_transaction->pending_callbacks, 0);
  }
}

TEST_F(TransactionEngineTest, VerifyNonDispatchedFailureNextPhases) {
  vector<TransactionPhase> current_phases = {TransactionPhase::Aborted,
                                             TransactionPhase::Unknown,
                                             TransactionPhase::Unknown};

  vector<TransactionPhase> previous_phases = {TransactionPhase::Begin,
                                              TransactionPhase::Committed,
                                              TransactionPhase::Aborted};

  atomic<bool> condition = false;
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<Transaction> current_transaction;

  mock_transaction_engine.log_transaction_and_proceed_to_next_phase_mock =
      [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ProceedToNextPhase(phase, transaction);
        return SuccessExecutionResult();
      };

  mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
      [&](TransactionPhase transaction_phase,
          shared_ptr<Transaction>& transaction) {
        mock_transaction_engine.ExecuteDistributedPhase(transaction_phase,
                                                        transaction);
        return SuccessExecutionResult();
      };

  for (size_t i = 0; i < previous_phases.size(); ++i) {
    mock_transaction_engine.begin_transaction_mock =
        [&](shared_ptr<Transaction>& transaction) mutable {
          if (previous_phases[i] == TransactionPhase::Begin) {
            mock_transaction_engine.aborted_transaction_mock =
                [&](shared_ptr<Transaction>& transaction) {
                  current_transaction = transaction;
                  condition = true;
                };
          }

          if (previous_phases[i] == TransactionPhase::Committed) {
            mock_transaction_engine.unknown_state_transaction_mock =
                [&](shared_ptr<Transaction>& transaction) {
                  current_transaction = transaction;
                  condition = true;
                };
          }

          if (previous_phases[i] == TransactionPhase::Aborted) {
            mock_transaction_engine.unknown_state_transaction_mock =
                [&](shared_ptr<Transaction>& transaction) {
                  current_transaction = transaction;
                  condition = true;
                };
          }

          transaction->current_phase_execution_result =
              FailureExecutionResult(1);
          transaction->current_phase = previous_phases[i];
          mock_transaction_engine.ProceedToNextPhase(previous_phases[i],
                                                     transaction);
        };

    AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
    transaction_context.request = make_shared<TransactionRequest>();
    transaction_context.request->transaction_id = Uuid::GenerateUuid();
    mock_transaction_engine.Execute(transaction_context);
    WaitUntil([&condition]() { return condition.load(); });

    EXPECT_EQ(current_transaction->current_phase, current_phases[i]);
    EXPECT_THAT(current_transaction->current_phase_execution_result,
                ResultIs(FailureExecutionResult(1)));
    EXPECT_EQ(current_transaction->pending_callbacks, 0);
  }
}

TEST_F(TransactionEngineTest, ConvertProtoPhaseToTransactionPhase) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::NOT_STARTED),
            transaction_manager::TransactionPhase::NotStarted);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::BEGIN),
            transaction_manager::TransactionPhase::Begin);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::PREPARE),
            transaction_manager::TransactionPhase::Prepare);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::COMMIT),
            transaction_manager::TransactionPhase::Commit);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::COMMITTED),
            transaction_manager::TransactionPhase::Committed);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::COMMIT_NOTIFY),
            transaction_manager::TransactionPhase::CommitNotify);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::ABORT_NOTIFY),
            transaction_manager::TransactionPhase::AbortNotify);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::ABORTED),
            transaction_manager::TransactionPhase::Aborted);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::END),
            transaction_manager::TransactionPhase::End);
  EXPECT_EQ(mock_transaction_engine.ConvertProtoPhaseToPhase(
                transaction_manager::proto::TransactionPhase::
                    TRANSACTION_PHASE_TYPE_UNKNOWN),
            transaction_manager::TransactionPhase::Unknown);
}

TEST_F(TransactionEngineTest, ConvertTransactionPhaseToProtoPhase) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::NotStarted),
            transaction_manager::proto::TransactionPhase::NOT_STARTED);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::Begin),
            transaction_manager::proto::TransactionPhase::BEGIN);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::Prepare),
            transaction_manager::proto::TransactionPhase::PREPARE);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::Commit),
            transaction_manager::proto::TransactionPhase::COMMIT);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::CommitNotify),
            transaction_manager::proto::TransactionPhase::COMMIT_NOTIFY);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::Committed),
            transaction_manager::proto::TransactionPhase::COMMITTED);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::AbortNotify),
            transaction_manager::proto::TransactionPhase::ABORT_NOTIFY);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::Aborted),
            transaction_manager::proto::TransactionPhase::ABORTED);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::End),
            transaction_manager::proto::TransactionPhase::END);
  EXPECT_EQ(mock_transaction_engine.ConvertPhaseToProtoPhase(
                transaction_manager::TransactionPhase::Unknown),
            transaction_manager::proto::TransactionPhase::
                TRANSACTION_PHASE_TYPE_UNKNOWN);
}

TEST_F(TransactionEngineTest, OnJournalServiceRecoverCallbackInvalidLog) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  auto bytes_buffer = make_shared<BytesBuffer>(1);
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED)));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackInvalidLogVersion) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(130);
  transaction_engine_log.mutable_version()->set_minor(432);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_SERIALIZATION_VERSION_IS_INVALID)));
}

TEST_F(TransactionEngineTest, OnJournalServiceRecoverCallbackInvalidLog1_0) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  BytesBuffer transaction_log_1_0_bytes_buffer(10);
  transaction_engine_log.set_log_body(
      transaction_log_1_0_bytes_buffer.bytes->data(),
      transaction_log_1_0_bytes_buffer.length);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED)));
}

TEST_F(TransactionEngineTest, OnJournalServiceRecoverCallbackInvalidLogType) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(
      TransactionLogType::TRANSACTION_LOG_TYPE_UNKNOWN);

  BytesBuffer transaction_engine_log_1_0_body_bytes_buffer(10);
  transaction_engine_log_1_0.set_log_body(
      transaction_engine_log_1_0_body_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_body_bytes_buffer.capacity);

  size_t bytes_serialized = 0;
  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());

  Serialization::SerializeProtoMessage<TransactionEngineLog_1_0>(
      transaction_engine_log_1_0_bytes_buffer, 0, transaction_engine_log_1_0,
      bytes_serialized);
  transaction_engine_log_1_0_bytes_buffer.length = bytes_serialized;

  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_LOG)));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackValidLogTypeInvalidBody) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(TransactionLogType::TRANSACTION_LOG);

  BytesBuffer transaction_engine_log_1_0_body_bytes_buffer(10);
  transaction_engine_log_1_0.set_log_body(
      transaction_engine_log_1_0_body_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_body_bytes_buffer.capacity);

  size_t bytes_serialized = 0;
  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());

  Serialization::SerializeProtoMessage<TransactionEngineLog_1_0>(
      transaction_engine_log_1_0_bytes_buffer, 0, transaction_engine_log_1_0,
      bytes_serialized);
  transaction_engine_log_1_0_bytes_buffer.length = bytes_serialized;

  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED)));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackValidLogTypeInvalidBody1) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(
      TransactionLogType::TRANSACTION_PHASE_LOG);

  BytesBuffer transaction_engine_log_1_0_body_bytes_buffer(10);
  transaction_engine_log_1_0.set_log_body(
      transaction_engine_log_1_0_body_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_body_bytes_buffer.capacity);

  size_t bytes_serialized = 0;
  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());

  Serialization::SerializeProtoMessage<TransactionEngineLog_1_0>(
      transaction_engine_log_1_0_bytes_buffer, 0, transaction_engine_log_1_0,
      bytes_serialized);
  transaction_engine_log_1_0_bytes_buffer.length = bytes_serialized;

  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED)));
}

TEST_F(TransactionEngineTest, OnJournalServiceRecoverCallbackInvalidCommand) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  auto mock_transaction_command_serializer =
      make_shared<MockTransactionCommandSerializer>();

  mock_transaction_command_serializer->deserialize_mock =
      [](const Uuid&, const BytesBuffer&, shared_ptr<TransactionCommand>&) {
        return FailureExecutionResult(123);
      };

  auto transaction_command_serializer =
      static_pointer_cast<TransactionCommandSerializerInterface>(
          mock_transaction_command_serializer);

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);

  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(TransactionLogType::TRANSACTION_LOG);

  TransactionLog_1_0 transaction_log_1_0;
  transaction_log_1_0.set_timeout(10000);
  transaction_log_1_0.mutable_id()->set_high(123);
  transaction_log_1_0.mutable_id()->set_low(456);

  auto command = transaction_log_1_0.add_commands();
  BytesBuffer command_body("invalid_command_buffer");
  command->set_command_body(command_body.bytes->data(), command_body.length);

  BytesBuffer transaction_log_1_bytes_buffer(
      transaction_log_1_0.ByteSizeLong());
  transaction_log_1_0.SerializeToArray(
      transaction_log_1_bytes_buffer.bytes->data(),
      transaction_log_1_bytes_buffer.capacity);
  transaction_log_1_bytes_buffer.length = transaction_log_1_0.ByteSizeLong();

  transaction_engine_log_1_0.set_log_body(
      transaction_log_1_bytes_buffer.bytes->data(),
      transaction_log_1_bytes_buffer.length);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED)));
}

TEST_F(TransactionEngineTest, OnJournalServiceRecoverCallbackValidCommand) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(TransactionLogType::TRANSACTION_LOG);

  TransactionLog_1_0 transaction_log_1_0;
  transaction_log_1_0.set_timeout(10000);
  transaction_log_1_0.mutable_id()->set_high(123);
  transaction_log_1_0.mutable_id()->set_low(456);
  transaction_log_1_0.set_is_coordinated_remotely(true);
  transaction_log_1_0.set_transaction_secret("This is the transaction secret");
  transaction_log_1_0.set_transaction_origin("origin.com");

  auto command = transaction_log_1_0.add_commands();
  BytesBuffer command_body(10);
  command->set_command_body(command_body.bytes->data(), command_body.length);

  command = transaction_log_1_0.add_commands();
  command->set_command_body(command_body.bytes->data(), command_body.length);

  BytesBuffer transaction_log_1_bytes_buffer(
      transaction_log_1_0.ByteSizeLong());
  transaction_log_1_0.SerializeToArray(
      transaction_log_1_bytes_buffer.bytes->data(),
      transaction_log_1_bytes_buffer.capacity);
  transaction_log_1_bytes_buffer.length = transaction_log_1_0.ByteSizeLong();

  transaction_engine_log_1_0.set_log_body(
      transaction_log_1_bytes_buffer.bytes->data(),
      transaction_log_1_bytes_buffer.length);

  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());
  transaction_engine_log_1_0.SerializeToArray(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.capacity);
  transaction_engine_log_1_0_bytes_buffer.length =
      transaction_engine_log_1_0.ByteSizeLong();

  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  mock_transaction_engine.initialize_transaction_mock =
      [&](AsyncContext<TransactionRequest, TransactionResponse>&
              transaction_context,
          shared_ptr<Transaction>& transaction) {
        EXPECT_EQ(transaction_context.request->transaction_id.high,
                  transaction_log_1_0.id().high());
        EXPECT_EQ(transaction_context.request->transaction_id.low,
                  transaction_log_1_0.id().low());
        EXPECT_EQ(transaction_context.request->timeout_time, 10000);
        EXPECT_EQ(transaction_context.request->commands.size(), 2);
        EXPECT_EQ(transaction_context.request->is_coordinated_remotely, true);
        EXPECT_EQ(*transaction_context.request->transaction_secret,
                  "This is the transaction secret");
        EXPECT_EQ(*transaction_context.request->transaction_origin,
                  "origin.com");
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(mock_transaction_engine.OnJournalServiceRecoverCallback(
      bytes_buffer, kDefaultUuid));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackTransactionNotFound) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  GetSampleTransactionPhaseLogBytes(
                      Uuid::GenerateUuid(),
                      transaction_manager::proto::TransactionPhase::COMMIT),
                  kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  core::errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND)));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackTransactionNotFoundOnEndPhase) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  GetSampleTransactionPhaseLogBytes(
                      Uuid::GenerateUuid(),
                      transaction_manager::proto::TransactionPhase::END),
                  kDefaultUuid),
              ResultIs(SuccessExecutionResult()));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackTransactionNotFoundSkipLog) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->SetBool(kTransactionManagerSkipFailedLogsInRecovery,
                                true);

  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client, mock_config_provider);
  EXPECT_THAT(mock_transaction_engine.Init(),
              ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  GetSampleTransactionPhaseLogBytes(
                      Uuid::GenerateUuid(),
                      transaction_manager::proto::TransactionPhase::COMMIT),
                  kDefaultUuid),
              ResultIs(SuccessExecutionResult()));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackTransactionPhaseInvalid) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);

  Uuid transaction_id = Uuid::GenerateUuid();
  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  GetSampleTransactionPhaseLogBytes(
                      transaction_id,
                      transaction_manager::proto::TransactionPhase::COMMIT),
                  kDefaultUuid),
              ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  GetSampleTransactionPhaseLogBytes(
                      transaction_id,
                      transaction_manager::proto::TransactionPhase::PREPARE),
                  kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_LOG)));
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackTransactionPhaseInvalidSkipLog) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->SetBool(kTransactionManagerSkipFailedLogsInRecovery,
                                true);

  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client, mock_config_provider);

  Uuid transaction_id = Uuid::GenerateUuid();
  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  EXPECT_THAT(mock_transaction_engine.Init(),
              ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  GetSampleTransactionPhaseLogBytes(
                      transaction_id,
                      transaction_manager::proto::TransactionPhase::COMMIT),
                  kDefaultUuid),
              ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  GetSampleTransactionPhaseLogBytes(
                      transaction_id,
                      transaction_manager::proto::TransactionPhase::PREPARE),
                  kDefaultUuid),
              ResultIs(SuccessExecutionResult()));
}

TEST_F(TransactionEngineTest, OnJournalServiceRecoverCallbackTransactionFound) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(
      TransactionLogType::TRANSACTION_PHASE_LOG);

  TransactionPhaseLog_1_0 transaction_phase_log_1_0;
  transaction_phase_log_1_0.mutable_id()->set_high(123);
  transaction_phase_log_1_0.mutable_id()->set_low(456);
  transaction_phase_log_1_0.set_phase(
      transaction_manager::proto::TransactionPhase::COMMIT);
  transaction_phase_log_1_0.set_failed(true);
  transaction_phase_log_1_0.mutable_result()->set_status(
      common::proto::ExecutionStatus::EXECUTION_STATUS_FAILURE);
  transaction_phase_log_1_0.mutable_result()->set_status_code(123);

  BytesBuffer transaction_phase_log_1_0_bytes_buffer(
      transaction_phase_log_1_0.ByteSizeLong());
  transaction_phase_log_1_0.SerializeToArray(
      transaction_phase_log_1_0_bytes_buffer.bytes->data(),
      transaction_phase_log_1_0_bytes_buffer.capacity);
  transaction_phase_log_1_0_bytes_buffer.length =
      transaction_phase_log_1_0.ByteSizeLong();

  transaction_engine_log_1_0.set_log_body(
      transaction_phase_log_1_0_bytes_buffer.bytes->data(),
      transaction_phase_log_1_0_bytes_buffer.length);

  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());
  transaction_engine_log_1_0.SerializeToArray(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.capacity);
  transaction_engine_log_1_0_bytes_buffer.length =
      transaction_engine_log_1_0.ByteSizeLong();

  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  Uuid transaction_id;
  transaction_id.low = 456;
  transaction_id.high = 123;
  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  EXPECT_SUCCESS(mock_transaction_engine.OnJournalServiceRecoverCallback(
      bytes_buffer, kDefaultUuid));

  EXPECT_EQ(transaction->current_phase, TransactionPhase::Commit);
  EXPECT_THAT(transaction->transaction_execution_result,
              ResultIs(FailureExecutionResult(123)));
  EXPECT_EQ(transaction->transaction_failed, true);
}

TEST_F(TransactionEngineTest,
       OnJournalServiceRecoverCallbackEndedTransactionMustBeRemoved) {
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(1);
  transaction_engine_log.mutable_version()->set_minor(0);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(
      TransactionLogType::TRANSACTION_PHASE_LOG);

  TransactionPhaseLog_1_0 transaction_phase_log_1_0;
  transaction_phase_log_1_0.mutable_id()->set_high(123);
  transaction_phase_log_1_0.mutable_id()->set_low(456);
  transaction_phase_log_1_0.set_phase(
      transaction_manager::proto::TransactionPhase::END);
  transaction_phase_log_1_0.set_failed(true);
  transaction_phase_log_1_0.mutable_result()->set_status(
      common::proto::ExecutionStatus::EXECUTION_STATUS_FAILURE);
  transaction_phase_log_1_0.mutable_result()->set_status_code(123);

  BytesBuffer transaction_phase_log_1_0_bytes_buffer(
      transaction_phase_log_1_0.ByteSizeLong());
  transaction_phase_log_1_0.SerializeToArray(
      transaction_phase_log_1_0_bytes_buffer.bytes->data(),
      transaction_phase_log_1_0_bytes_buffer.capacity);
  transaction_phase_log_1_0_bytes_buffer.length =
      transaction_phase_log_1_0.ByteSizeLong();

  transaction_engine_log_1_0.set_log_body(
      transaction_phase_log_1_0_bytes_buffer.bytes->data(),
      transaction_phase_log_1_0_bytes_buffer.length);

  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());
  transaction_engine_log_1_0.SerializeToArray(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.capacity);
  transaction_engine_log_1_0_bytes_buffer.length =
      transaction_engine_log_1_0.ByteSizeLong();

  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(transaction_engine_log.ByteSizeLong());
  Serialization::SerializeProtoMessage<TransactionEngineLog>(
      *bytes_buffer, 0, transaction_engine_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  Uuid transaction_id;
  transaction_id.low = 456;
  transaction_id.high = 123;
  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Find(
                transaction_id, transaction),
            SuccessExecutionResult());

  EXPECT_SUCCESS(mock_transaction_engine.OnJournalServiceRecoverCallback(
      bytes_buffer, kDefaultUuid));

  EXPECT_EQ(
      mock_transaction_engine.GetActiveTransactionsMap().Find(transaction_id,
                                                              transaction),
      FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));
}

TEST_F(TransactionEngineTest, LogTransactionAndProceedToNextPhase) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  transaction_context.request->is_coordinated_remotely = true;
  transaction_context.request->transaction_secret = make_shared<string>("1234");
  transaction_context.request->transaction_origin =
      make_shared<string>("origin.com");
  transaction->context = transaction_context;
  transaction->current_phase = TransactionPhase::AbortNotify;

  mock_journal_service->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) {
        EXPECT_EQ(journal_log_context.request->component_id.high, 0xFFFFFFF1);
        EXPECT_EQ(journal_log_context.request->component_id.low, 0x00000004);
        EXPECT_EQ(journal_log_context.request->log_status,
                  JournalLogStatus::Log);
        EXPECT_NE(journal_log_context.request->data->length, 0);
        EXPECT_NE(journal_log_context.request->data->capacity, 0);
        EXPECT_EQ(mock_transaction_engine.OnJournalServiceRecoverCallback(
                      journal_log_context.request->data, kDefaultUuid),
                  SuccessExecutionResult());
        return SuccessExecutionResult();
      };

  EXPECT_EQ(mock_transaction_engine.LogTransactionAndProceedToNextPhase(
                TransactionPhase::AbortNotify, transaction),
            SuccessExecutionResult());
}

TEST_F(TransactionEngineTest, LogStateAndProceedToNextPhase) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  transaction->context = transaction_context;
  transaction->current_phase = TransactionPhase::AbortNotify;
  transaction->current_phase_execution_result = FailureExecutionResult(123);
  transaction->current_phase_failed = true;
  transaction->id = transaction_context.request->transaction_id;
  transaction->pending_callbacks = 10;
  transaction->transaction_execution_result = FailureExecutionResult(423);
  transaction->transaction_failed = true;

  auto pair =
      make_pair(transaction_context.request->transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);

  mock_journal_service->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) {
        EXPECT_EQ(journal_log_context.request->component_id.high, 0xFFFFFFF1);
        EXPECT_EQ(journal_log_context.request->component_id.low, 0x00000004);
        EXPECT_EQ(journal_log_context.request->log_status,
                  JournalLogStatus::Log);
        EXPECT_NE(journal_log_context.request->data->length, 0);
        EXPECT_NE(journal_log_context.request->data->capacity, 0);
        EXPECT_EQ(mock_transaction_engine.OnJournalServiceRecoverCallback(
                      journal_log_context.request->data, kDefaultUuid),
                  SuccessExecutionResult());
        return SuccessExecutionResult();
      };

  EXPECT_EQ(mock_transaction_engine.LogStateAndProceedToNextPhase(
                TransactionPhase::AbortNotify, transaction),
            SuccessExecutionResult());
}

TEST_F(TransactionEngineTest, SerializeTransaction) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  transaction_context.request->transaction_secret = make_shared<string>();
  transaction_context.request->transaction_origin = make_shared<string>();

  transaction->context = transaction_context;
  transaction->current_phase = TransactionPhase::AbortNotify;
  transaction->current_phase_execution_result = FailureExecutionResult(123);
  transaction->current_phase_failed = true;
  transaction->id = transaction_context.request->transaction_id;
  transaction->pending_callbacks = 10;
  transaction->transaction_execution_result = FailureExecutionResult(423);
  transaction->transaction_failed = true;

  auto bytes_buffer = make_shared<BytesBuffer>();
  EXPECT_SUCCESS(
      mock_transaction_engine.SerializeTransaction(transaction, *bytes_buffer));

  EXPECT_NE(bytes_buffer->length, 0);
  EXPECT_NE(bytes_buffer->capacity, 0);
  EXPECT_SUCCESS(mock_transaction_engine.OnJournalServiceRecoverCallback(
      bytes_buffer, kDefaultUuid));
}

TEST_F(TransactionEngineTest, SerializeState) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  shared_ptr<Transaction> transaction = make_shared<Transaction>();
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  transaction->context = transaction_context;
  transaction->current_phase = TransactionPhase::AbortNotify;
  transaction->current_phase_execution_result = FailureExecutionResult(123);
  transaction->current_phase_failed = true;
  transaction->id = transaction_context.request->transaction_id;
  transaction->pending_callbacks = 10;
  transaction->transaction_execution_result = FailureExecutionResult(423);
  transaction->transaction_failed = true;

  auto bytes_buffer = make_shared<BytesBuffer>();
  EXPECT_SUCCESS(
      mock_transaction_engine.SerializeState(transaction, *bytes_buffer));

  EXPECT_NE(bytes_buffer->length, 0);
  EXPECT_NE(bytes_buffer->capacity, 0);
  EXPECT_THAT(mock_transaction_engine.OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              ResultIs(FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND)));

  auto pair =
      make_pair(transaction_context.request->transaction_id, transaction);
  mock_transaction_engine.GetActiveTransactionsMap().Insert(pair, transaction);
  EXPECT_SUCCESS(mock_transaction_engine.OnJournalServiceRecoverCallback(
      bytes_buffer, kDefaultUuid));
}

TEST_F(TransactionEngineTest, ProceedToNextPhaseAfterRecovery) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  vector<TransactionPhase> current_phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown};

  vector<TransactionPhase> next_phases = {
      TransactionPhase::Begin,        TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown};

  for (size_t i = 0; i < current_phases.size(); ++i) {
    auto called = false;
    mock_transaction_engine.execute_distributed_phase_mock =
        [&](TransactionPhase next_phase, shared_ptr<Transaction>& transaction) {
          EXPECT_EQ(next_phase, next_phases[i]);
          EXPECT_SUCCESS(transaction->current_phase_execution_result);
          EXPECT_EQ(transaction->current_phase_failed.load(), false);
          called = true;
        };
    mock_transaction_engine.log_state_and_execute_distributed_phase_mock =
        [&](TransactionPhase next_phase, shared_ptr<Transaction>& transaction) {
          EXPECT_EQ(next_phase, next_phases[i]);
          EXPECT_SUCCESS(transaction->current_phase_execution_result);
          EXPECT_EQ(transaction->current_phase_failed.load(), false);
          called = true;
          return SuccessExecutionResult();
        };
    mock_transaction_engine.log_state_and_proceed_to_next_phase_mock =
        [&](TransactionPhase next_phase, shared_ptr<Transaction>& transaction) {
          EXPECT_EQ(next_phase, next_phases[i]);
          EXPECT_SUCCESS(transaction->current_phase_execution_result);
          EXPECT_EQ(transaction->current_phase_failed.load(), false);
          called = true;
          return SuccessExecutionResult();
        };

    mock_transaction_engine.proceed_to_next_phase_mock =
        [&](TransactionPhase phase, shared_ptr<Transaction>& transaction) {
          EXPECT_EQ(phase, current_phases[i]);
          EXPECT_SUCCESS(transaction->current_phase_execution_result);
          EXPECT_EQ(transaction->current_phase_failed.load(), false);
          called = true;
          return SuccessExecutionResult();
        };

    auto transaction = make_shared<Transaction>();
    transaction->current_phase = current_phases[i];
    transaction->context.request = make_shared<TransactionRequest>();

    mock_transaction_engine.ProceedToNextPhaseAfterRecovery(transaction);

    if (current_phases[i] != TransactionPhase::Unknown) {
      EXPECT_EQ(called, true);
    } else {
      EXPECT_EQ(called, false);
    }
  }
}

TEST_F(TransactionEngineTest, Checkpoint) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  auto checkpoint_logs = make_shared<list<CheckpointLog>>();
  mock_transaction_engine.Checkpoint(checkpoint_logs);
  EXPECT_EQ(checkpoint_logs->size(), 0);

  Uuid transaction_id_1 = {.high = 1, .low = 1};
  auto transaction_1 = make_shared<Transaction>();
  transaction_1->id = transaction_id_1;
  transaction_1->current_phase = TransactionPhase::Commit;
  transaction_1->context.request = make_shared<TransactionRequest>();
  transaction_1->context.request->timeout_time = 1234567;
  transaction_1->context.request->is_coordinated_remotely = true;
  transaction_1->context.request->transaction_secret =
      make_shared<string>("This is the secret");
  transaction_1->context.request->transaction_origin =
      make_shared<string>("origin.com");
  transaction_1->is_coordinated_remotely = true;
  transaction_1->transaction_secret = make_shared<string>("This is the secret");
  transaction_1->transaction_origin = make_shared<string>("origin.com");
  auto pair_1 = make_pair(transaction_id_1, transaction_1);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair_1, transaction_1),
            SuccessExecutionResult());

  Uuid transaction_id_2 = {.high = 3, .low = 1};
  auto transaction_2 = make_shared<Transaction>();
  transaction_2->id = transaction_id_2;
  transaction_2->current_phase = TransactionPhase::CommitNotify;
  auto pair_2 = make_pair(transaction_id_2, transaction_2);
  transaction_2->context.request = make_shared<TransactionRequest>();
  transaction_2->context.request->timeout_time = 12346412;
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair_2, transaction_2),
            SuccessExecutionResult());

  Uuid transaction_id_3 = {.high = 2, .low = 1};
  auto transaction_3 = make_shared<Transaction>();
  transaction_3->id = transaction_id_3;
  transaction_3->current_phase = TransactionPhase::AbortNotify;
  transaction_3->context.request = make_shared<TransactionRequest>();
  transaction_3->context.request->timeout_time = 635635;
  auto pair_3 = make_pair(transaction_id_3, transaction_3);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair_3, transaction_3),
            SuccessExecutionResult());

  EXPECT_SUCCESS(mock_transaction_engine.Checkpoint(checkpoint_logs));
  EXPECT_EQ(checkpoint_logs->size(), 6);

  MockTransactionEngine mock_transaction_engine_for_recovery(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  Uuid transaction_manager_id = {.high = 0xFFFFFFF1, .low = 0x00000004};
  auto it = checkpoint_logs->begin();
  EXPECT_EQ(it->component_id, transaction_manager_id);
  EXPECT_NE(it->log_id, common::kZeroUuid);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  auto bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(
      mock_transaction_engine_for_recovery.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  it++;
  EXPECT_EQ(it->component_id, transaction_manager_id);
  EXPECT_NE(it->log_id, common::kZeroUuid);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(
      mock_transaction_engine_for_recovery.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  it++;
  EXPECT_EQ(it->component_id, transaction_manager_id);
  EXPECT_NE(it->log_id, common::kZeroUuid);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(
      mock_transaction_engine_for_recovery.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  it++;
  EXPECT_EQ(it->component_id, transaction_manager_id);
  EXPECT_NE(it->log_id, common::kZeroUuid);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(
      mock_transaction_engine_for_recovery.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  it++;
  EXPECT_EQ(it->component_id, transaction_manager_id);
  EXPECT_NE(it->log_id, common::kZeroUuid);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(
      mock_transaction_engine_for_recovery.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  it++;
  EXPECT_EQ(it->component_id, transaction_manager_id);
  EXPECT_NE(it->log_id, common::kZeroUuid);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(
      mock_transaction_engine_for_recovery.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  it++;
  EXPECT_EQ(it, checkpoint_logs->end());

  vector<Uuid> transaction_ids;
  vector<Uuid> checkpoint_transaction_ids;
  mock_transaction_engine.GetActiveTransactionsMap().Keys(transaction_ids);
  mock_transaction_engine_for_recovery.GetActiveTransactionsMap().Keys(
      checkpoint_transaction_ids);

  EXPECT_EQ(transaction_ids.size(), 3);
  EXPECT_EQ(checkpoint_transaction_ids.size(), 3);

  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(transaction_ids[i], checkpoint_transaction_ids[i]);
  }

  for (auto transaction_id : transaction_ids) {
    shared_ptr<Transaction> original_transaction;
    shared_ptr<Transaction> checkpoint_transaction;

    mock_transaction_engine.GetActiveTransactionsMap().Find(
        transaction_id, original_transaction);
    mock_transaction_engine_for_recovery.GetActiveTransactionsMap().Find(
        transaction_id, checkpoint_transaction);

    EXPECT_EQ(original_transaction->id.low, checkpoint_transaction->id.low);
    EXPECT_EQ(original_transaction->id.high, checkpoint_transaction->id.high);
    EXPECT_EQ(original_transaction->current_phase,
              checkpoint_transaction->current_phase);
    EXPECT_EQ(original_transaction->current_phase_execution_result,
              checkpoint_transaction->current_phase_execution_result);
    EXPECT_EQ(original_transaction->pending_callbacks,
              checkpoint_transaction->pending_callbacks);
    EXPECT_EQ(original_transaction->is_coordinated_remotely,
              checkpoint_transaction->is_coordinated_remotely);
    if (original_transaction->is_coordinated_remotely) {
      EXPECT_EQ(*original_transaction->transaction_secret,
                *checkpoint_transaction->transaction_secret);
      EXPECT_EQ(*original_transaction->transaction_origin,
                *checkpoint_transaction->transaction_origin);
    }
    EXPECT_EQ(original_transaction->is_waiting_for_remote,
              checkpoint_transaction->is_waiting_for_remote);
  }
}

TEST_F(TransactionEngineTest, LockRemotelyCoordinatedTransaction) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  auto transaction = make_shared<Transaction>();
  transaction->is_coordinated_remotely = false;

  EXPECT_THAT(
      mock_transaction_engine.LockRemotelyCoordinatedTransaction(transaction),
      ResultIs(FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY)));

  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  EXPECT_THAT(
      mock_transaction_engine.LockRemotelyCoordinatedTransaction(transaction),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_CANNOT_BE_LOCKED)));

  transaction->is_waiting_for_remote = true;
  EXPECT_SUCCESS(
      mock_transaction_engine.LockRemotelyCoordinatedTransaction(transaction));
  EXPECT_EQ(transaction->is_waiting_for_remote.load(), false);
}

TEST_F(TransactionEngineTest, UnlockRemotelyCoordinatedTransaction) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  auto transaction = make_shared<Transaction>();
  transaction->is_coordinated_remotely = false;

  EXPECT_THAT(
      mock_transaction_engine.UnlockRemotelyCoordinatedTransaction(transaction),
      ResultIs(FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY)));

  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = true;
  EXPECT_THAT(
      mock_transaction_engine.UnlockRemotelyCoordinatedTransaction(transaction),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_CANNOT_BE_UNLOCKED)));

  transaction->is_waiting_for_remote = false;
  EXPECT_SUCCESS(mock_transaction_engine.UnlockRemotelyCoordinatedTransaction(
      transaction));
  EXPECT_EQ(transaction->is_waiting_for_remote.load(), true);
}

TEST_F(TransactionEngineTest, ResolveNonRemotelyCoordinatedTransaction) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  vector<TransactionPhase> cancellable_phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify,
  };

  vector<TransactionPhase> non_cancellable_phases = {
      TransactionPhase::Committed,
      TransactionPhase::AbortNotify,
      TransactionPhase::Aborted,
      TransactionPhase::End,
  };

  for (auto cancellable_phase : cancellable_phases) {
    auto transaction = make_shared<Transaction>();
    transaction->is_coordinated_remotely = false;
    transaction->current_phase = cancellable_phase;
    transaction->current_phase_failed = false;
    transaction->current_phase_execution_result = SuccessExecutionResult();
    transaction->expiration_time = 0;
    atomic<bool> called = false;
    mock_transaction_engine.proceed_to_next_phase_mock =
        [&](TransactionPhase, shared_ptr<Transaction>&) { called = true; };
    EXPECT_SUCCESS(mock_transaction_engine.ResolveTransaction(transaction));
    EXPECT_EQ(called.load(), true);
    EXPECT_THAT(transaction->current_phase_execution_result,
                ResultIs(FailureExecutionResult(
                    errors::SC_TRANSACTION_MANAGER_TRANSACTION_TIMEOUT)));
    EXPECT_EQ(transaction->current_phase_failed.load(), true);
  }

  for (auto non_cancellable_phase : non_cancellable_phases) {
    auto transaction = make_shared<Transaction>();
    transaction->is_coordinated_remotely = false;
    transaction->current_phase = non_cancellable_phase;
    transaction->current_phase_failed = false;
    transaction->current_phase_execution_result = SuccessExecutionResult();
    transaction->expiration_time = 0;
    atomic<bool> called = false;
    mock_transaction_engine.proceed_to_next_phase_mock =
        [&](TransactionPhase, shared_ptr<Transaction>&) { called = true; };
    EXPECT_SUCCESS(mock_transaction_engine.ResolveTransaction(transaction));
    EXPECT_EQ(called.load(), true);
    EXPECT_SUCCESS(transaction->current_phase_execution_result);
    EXPECT_EQ(transaction->current_phase_failed.load(), false);
  }
}

TEST_F(TransactionEngineTest,
       ResolveRemotelyCoordinatedTransactionPendingCallback) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  auto transaction = make_shared<Transaction>();
  transaction->is_coordinated_remotely = true;
  transaction->pending_callbacks = 3;
  transaction->expiration_time = 0;

  EXPECT_THAT(
      mock_transaction_engine.ResolveTransaction(transaction),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_HAS_PENDING_CALLBACKS)));
}

TEST_F(TransactionEngineTest,
       ResolveRemotelyCoordinatedTransactionAlreadyLocked) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutorWithInternals>(2, 100);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  auto transaction = make_shared<Transaction>();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->expiration_time = 0;

  EXPECT_THAT(
      mock_transaction_engine.ResolveTransaction(transaction),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_CANNOT_BE_LOCKED)));
}

TEST_F(TransactionEngineTest,
       ResolveRemotelyCoordinatedTransactionInquiryTheRemoteState) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = true;
  transaction->transaction_secret = make_shared<string>("this is the secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->expiration_time = 0;

  auto mock_remote_transaction_manager =
      make_shared<MockRemoteTransactionManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager =
      mock_remote_transaction_manager;

  atomic<bool> called = false;
  mock_remote_transaction_manager->get_transaction_status_mock =
      [&](AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>&
              get_transaction_status_context) {
        EXPECT_EQ(get_transaction_status_context.request->transaction_id,
                  transaction->id);
        EXPECT_EQ(*get_transaction_status_context.request->transaction_secret,
                  *transaction->transaction_secret);
        called = true;
        return SuccessExecutionResult();
      };
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      transaction_phase_manager, remote_transaction_manager, mock_metric_client,
      100000);

  EXPECT_SUCCESS(mock_transaction_engine.ResolveTransaction(transaction));
  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       OnGetRemoteTransactionStatusCallbackUnsuccessful) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result = FailureExecutionResult(123);

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;

  atomic<bool> called = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return called.load(); });
  called = false;

  mock_transaction_engine.on_remote_transaction_not_found =
      [&](auto& transaction, auto& context) { called = true; };

  get_transaction_status_context.result =
      FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST);

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);
  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest, OnGetRemoteTransactionStatusCallbackUnExpired) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result = SuccessExecutionResult();
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->is_expired = false;

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;

  atomic<bool> called = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](auto& transaction) {
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       OnGetRemoteTransactionStatusCallbackOutOfSyncTransactions) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result = SuccessExecutionResult();
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->is_expired = true;
  get_transaction_status_context.response->transaction_execution_phase =
      TransactionExecutionPhase::Commit;

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->current_phase = TransactionPhase::End;

  atomic<bool> called = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](auto& transaction) {
        EXPECT_TRUE(transaction->blocked);
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       OnGetRemoteTransactionStatusCallbackTransactionAheadOfRemote) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result = SuccessExecutionResult();
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->is_expired = true;
  get_transaction_status_context.response->transaction_execution_phase =
      TransactionExecutionPhase::Commit;

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->current_phase = TransactionPhase::CommitNotify;

  atomic<bool> called = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](auto& transaction) {
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       OnGetRemoteTransactionStatusCallbackTransactionPhaseFailed) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result = SuccessExecutionResult();
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->is_expired = true;
  get_transaction_status_context.response->transaction_execution_phase =
      TransactionExecutionPhase::Notify;

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->current_phase = TransactionPhase::CommitNotify;
  transaction->current_phase_failed = true;

  atomic<bool> called = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](auto& transaction) {
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       OnGetRemoteTransactionStatusCallbackTransactionBehindOfRemote) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result = SuccessExecutionResult();
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->is_expired = true;
  get_transaction_status_context.response->has_failure = true;
  get_transaction_status_context.response->transaction_execution_phase =
      TransactionExecutionPhase::Notify;

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->current_phase = TransactionPhase::Commit;

  atomic<bool> called = false;
  mock_transaction_engine.roll_forward_local_transaction_mock =
      [&](std::shared_ptr<Transaction>& transaction_copy,
          TransactionExecutionPhase next_phase, bool phase_failed) {
        EXPECT_EQ(transaction_copy->id, transaction->id);
        EXPECT_EQ(next_phase, TransactionExecutionPhase::Notify);
        EXPECT_EQ(phase_failed, true);
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       OnGetRemoteTransactionStatusCallbackTransactionSameAsRemote) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result = SuccessExecutionResult();
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->is_expired = true;
  get_transaction_status_context.response->has_failure = true;
  get_transaction_status_context.response->last_execution_timestamp = 123456;
  get_transaction_status_context.response->transaction_execution_phase =
      TransactionExecutionPhase::Notify;

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->current_phase = TransactionPhase::CommitNotify;

  atomic<bool> called = false;
  mock_transaction_engine.roll_forward_local_and_remote_transactions_mock =
      [&](std::shared_ptr<Transaction>& transaction_copy,
          Timestamp remote_last_execution_time) {
        EXPECT_EQ(transaction_copy->id, transaction->id);
        EXPECT_EQ(remote_last_execution_time, 123456);
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnGetRemoteTransactionStatusCallback(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest, OnRemoteTransactionNotFound) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result =
      FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST);
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->transaction_secret = make_shared<string>("This is secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->last_execution_timestamp = 123456;

  vector<TransactionPhase> phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown,
  };
  auto mock_metric_client = make_shared<MockMetricClient>();
  for (auto phase : phases) {
    transaction->blocked = false;
    transaction->current_phase = phase;
    shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
    MockTransactionEngine mock_transaction_engine(
        async_executor, mock_transaction_command_serializer, journal_service,
        remote_transaction_manager, mock_metric_client);

    atomic<bool> unlock_called = false;
    atomic<bool> execute_phase_called = false;
    if (phase != TransactionPhase::Begin &&
        phase != TransactionPhase::Prepare &&
        phase != TransactionPhase::Aborted &&
        phase != TransactionPhase::Committed &&
        phase != TransactionPhase::End) {
      mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
          [&](auto& transaction) {
            EXPECT_TRUE(transaction->blocked);
            unlock_called = true;
            return SuccessExecutionResult();
          };
      mock_transaction_engine.execute_phase_internal_mock =
          [&](shared_ptr<Transaction>& transaction,
              AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            execute_phase_called = true;
            return FailureExecutionResult(SC_UNKNOWN);
          };

      mock_transaction_engine.OnRemoteTransactionNotFound(
          transaction, get_transaction_status_context);

      WaitUntil([&]() { return unlock_called.load(); });
      ASSERT_FALSE(execute_phase_called.load());
    } else {
      mock_transaction_engine.execute_phase_internal_mock =
          [&](shared_ptr<Transaction>& transaction,
              AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_FALSE(transaction->blocked);
            EXPECT_EQ(transaction_phase_context.request->transaction_id,
                      transaction->id);
            EXPECT_EQ(*transaction_phase_context.request->transaction_secret,
                      *transaction->transaction_secret);
            EXPECT_EQ(*transaction_phase_context.request->transaction_origin,
                      *transaction->transaction_origin);
            EXPECT_EQ(
                transaction_phase_context.request->transaction_execution_phase,
                TransactionExecutionPhase::End);
            EXPECT_EQ(
                transaction_phase_context.request->last_execution_timestamp,
                123456);

            execute_phase_called = true;
            return SuccessExecutionResult();
          };
      mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
          [&](auto& transaction) {
            EXPECT_FALSE(transaction->blocked);
            unlock_called = true;
            return FailureExecutionResult(SC_UNKNOWN);
          };

      mock_transaction_engine.OnRemoteTransactionNotFound(
          transaction, get_transaction_status_context);

      WaitUntil([&]() { return execute_phase_called.load(); });
      ASSERT_FALSE(unlock_called.load());
    }
  }
}

TEST_F(TransactionEngineTest, OnRemoteTransactionNotFoundUnlockTransaction) {
  // If transaction on remote is not found and the current transaction state
  // is movable to End, but if unable to move to next phase, we must unlock
  // the locked transaction.
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.result =
      FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST);
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->transaction_secret = make_shared<string>("This is secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->last_execution_timestamp = 123456;
  auto mock_metric_client = make_shared<MockMetricClient>();
  transaction->current_phase = TransactionPhase::End;

  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  atomic<bool> execute_phase_called = false;
  mock_transaction_engine.execute_phase_internal_mock =
      [&](shared_ptr<Transaction>& transaction,
          AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        execute_phase_called = true;
        return FailureExecutionResult(12345);
      };

  atomic<bool> unlocked = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        unlocked = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnRemoteTransactionNotFound(
      transaction, get_transaction_status_context);

  WaitUntil([&]() { return unlocked.load(); });
  ASSERT_TRUE(execute_phase_called.load());
}

TEST_F(TransactionEngineTest, RollForwardLocalTransaction) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->transaction_secret = make_shared<string>("This is secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->last_execution_timestamp = 123456;

  vector<TransactionPhase> phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown,
  };

  vector<TransactionExecutionPhase> next_phases = {
      TransactionExecutionPhase::Begin,  TransactionExecutionPhase::Prepare,
      TransactionExecutionPhase::Commit, TransactionExecutionPhase::Notify,
      TransactionExecutionPhase::Abort,  TransactionExecutionPhase::End,
  };

  vector<bool> next_phase_outcomes = {true, false};
  auto mock_metric_client = make_shared<MockMetricClient>();
  for (auto phase : phases) {
    for (auto next_phase : next_phases) {
      for (auto next_phase_outcome : next_phase_outcomes) {
        transaction->current_phase = phase;

        shared_ptr<RemoteTransactionManagerInterface>
            remote_transaction_manager;
        MockTransactionEngine mock_transaction_engine(
            async_executor, mock_transaction_command_serializer,
            journal_service, remote_transaction_manager, mock_metric_client);

        atomic<bool> unlocked = false;
        mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
            [&](shared_ptr<Transaction>& transaction) {
              unlocked = true;
              return SuccessExecutionResult();
            };

        atomic<bool> called = false;
        mock_transaction_engine.execute_phase_internal_mock =
            [&](shared_ptr<Transaction>& copied_transaction,
                AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                    transaction_phase_context) {
              EXPECT_EQ(copied_transaction->id, transaction->id);
              EXPECT_EQ(*copied_transaction->transaction_secret,
                        *transaction->transaction_secret);
              EXPECT_EQ(*copied_transaction->transaction_origin,
                        *transaction->transaction_origin);
              EXPECT_EQ(transaction_phase_context.request->transaction_id,
                        transaction->id);
              EXPECT_EQ(*transaction_phase_context.request->transaction_secret,
                        *transaction->transaction_secret);
              EXPECT_EQ(*transaction_phase_context.request->transaction_origin,
                        *transaction->transaction_origin);
              EXPECT_EQ(
                  transaction_phase_context.request->last_execution_timestamp,
                  transaction->last_execution_timestamp);

              if (phase != TransactionPhase::End &&
                  phase != TransactionPhase::Aborted &&
                  phase != TransactionPhase::Committed) {
                if (next_phase == TransactionExecutionPhase::Abort) {
                  EXPECT_EQ(transaction_phase_context.request
                                ->transaction_execution_phase,
                            TransactionExecutionPhase::Abort);

                } else if (next_phase == TransactionExecutionPhase::End &&
                           next_phase_outcome) {
                  EXPECT_EQ(transaction_phase_context.request
                                ->transaction_execution_phase,
                            TransactionExecutionPhase::Abort);
                } else {
                  EXPECT_EQ(transaction_phase_context.request
                                ->transaction_execution_phase,
                            mock_transaction_engine.ToTransactionExecutionPhase(
                                transaction->current_phase));
                }
              } else {
                EXPECT_EQ(transaction_phase_context.request
                              ->transaction_execution_phase,
                          mock_transaction_engine.ToTransactionExecutionPhase(
                              transaction->current_phase));
              }

              called = true;
              return FailureExecutionResult(12345);
            };

        mock_transaction_engine.RollForwardLocalTransaction(
            transaction, next_phase, next_phase_outcome);
        WaitUntil([&]() { return unlocked.load() && called.load(); });
      }
    }
  }
}

TEST_F(TransactionEngineTest, RollForwardLocalAndRemoteTransactions) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->transaction_secret = make_shared<string>("This is secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->last_execution_timestamp = 123456;

  vector<TransactionPhase> phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown,
  };

  for (auto phase : phases) {
    transaction->current_phase = phase;
    auto mock_remote_transaction_manager =
        make_shared<MockRemoteTransactionManager>();
    shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager =
        mock_remote_transaction_manager;
    auto mock_metric_client = make_shared<MockMetricClient>();
    atomic<bool> called = false;
    MockTransactionEngine mock_transaction_engine(
        async_executor, mock_transaction_command_serializer, journal_service,
        transaction_phase_manager, mock_remote_transaction_manager,
        mock_metric_client, 100000);

    mock_remote_transaction_manager->execute_phase_mock =
        [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                execute_transaction_status_context) {
          EXPECT_EQ(execute_transaction_status_context.request->transaction_id,
                    transaction->id);
          EXPECT_EQ(
              *execute_transaction_status_context.request->transaction_secret,
              *transaction->transaction_secret);
          EXPECT_EQ(
              *execute_transaction_status_context.request->transaction_origin,
              *transaction->transaction_origin);
          EXPECT_EQ(execute_transaction_status_context.request
                        ->last_execution_timestamp,
                    1234567);
          if (mock_transaction_engine.CanCancel(phase)) {
            EXPECT_EQ(execute_transaction_status_context.request
                          ->transaction_execution_phase,
                      TransactionExecutionPhase::Abort);
          } else {
            EXPECT_EQ(
                execute_transaction_status_context.request
                    ->transaction_execution_phase,
                mock_transaction_engine.ToTransactionExecutionPhase(phase));
          }
          called = true;
          return SuccessExecutionResult();
        };

    mock_transaction_engine.RollForwardLocalAndRemoteTransactions(transaction,
                                                                  1234567);
    WaitUntil([&]() { return called.load(); });
  }
}

TEST_F(TransactionEngineTest, OnRollForwardRemoteTransactionCallbackFailed) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->transaction_secret = make_shared<string>("This is secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->last_execution_timestamp = 123456;

  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      remote_transaction_phase_context;
  remote_transaction_phase_context.result = FailureExecutionResult(123);

  atomic<bool> called = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.OnRollForwardRemoteTransactionCallback(
      transaction, remote_transaction_phase_context);
  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       OnRollForwardRemoteTransactionCallbackSuccessful) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->transaction_secret = make_shared<string>("This is secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->last_execution_timestamp = 12343356;

  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      remote_transaction_phase_context;
  remote_transaction_phase_context.request =
      make_shared<TransactionPhaseRequest>();
  remote_transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Notify;
  remote_transaction_phase_context.result = SuccessExecutionResult();
  remote_transaction_phase_context.response =
      make_shared<TransactionPhaseResponse>();
  remote_transaction_phase_context.response->last_execution_timestamp =
      12348966;

  atomic<bool> unlocked = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        unlocked = true;
        return SuccessExecutionResult();
      };

  atomic<bool> called = false;
  mock_transaction_engine.execute_phase_internal_mock =
      [&](shared_ptr<Transaction>& transaction,
          AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        EXPECT_EQ(transaction_phase_context.request->transaction_id,
                  transaction->id);
        EXPECT_EQ(*transaction_phase_context.request->transaction_secret,
                  *transaction->transaction_secret);
        EXPECT_EQ(*transaction_phase_context.request->transaction_origin,
                  *transaction->transaction_origin);
        EXPECT_EQ(
            transaction_phase_context.request->transaction_execution_phase,
            remote_transaction_phase_context.request
                ->transaction_execution_phase);
        EXPECT_EQ(transaction_phase_context.request->last_execution_timestamp,
                  transaction->last_execution_timestamp);

        called = true;
        return FailureExecutionResult(1234);
      };

  mock_transaction_engine.OnRollForwardRemoteTransactionCallback(
      transaction, remote_transaction_phase_context);
  WaitUntil([&]() { return unlocked.load() && called.load(); });
}

TEST_F(TransactionEngineTest, ToTransactionPhase) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  vector<TransactionExecutionPhase> phases = {
      TransactionExecutionPhase::Begin,  TransactionExecutionPhase::Prepare,
      TransactionExecutionPhase::Commit, TransactionExecutionPhase::Notify,
      TransactionExecutionPhase::Abort,  TransactionExecutionPhase::End,
      TransactionExecutionPhase::Unknown};

  vector<TransactionPhase> expected_phases = {
      TransactionPhase::Begin,       TransactionPhase::Prepare,
      TransactionPhase::Commit,      TransactionPhase::CommitNotify,
      TransactionPhase::AbortNotify, TransactionPhase::End,
      TransactionPhase::Unknown,
  };

  for (size_t i = 0; i < phases.size(); ++i) {
    EXPECT_EQ(mock_transaction_engine.ToTransactionPhase(phases[i]),
              expected_phases[i]);
  }
}

TEST_F(TransactionEngineTest, LocalAndRemoteTransactionsInSync) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  vector<TransactionPhase> phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown,
  };

  for (size_t i = 0; i < phases.size(); ++i) {
    for (size_t j = 0; j < phases.size(); ++j) {
      bool is_in_sync = false;
      if (phases[i] == TransactionPhase::Begin) {
        if (phases[j] == TransactionPhase::NotStarted ||
            phases[j] == TransactionPhase::Begin ||
            phases[j] == TransactionPhase::Prepare ||
            phases[j] == TransactionPhase::AbortNotify) {
          is_in_sync = true;
        }
      }

      if (phases[i] == TransactionPhase::Prepare) {
        if (phases[j] == TransactionPhase::Begin ||
            phases[j] == TransactionPhase::Prepare ||
            phases[j] == TransactionPhase::Commit ||
            phases[j] == TransactionPhase::AbortNotify) {
          is_in_sync = true;
        }
      }

      if (phases[i] == TransactionPhase::Commit) {
        if (phases[j] == TransactionPhase::Prepare ||
            phases[j] == TransactionPhase::Commit ||
            phases[j] == TransactionPhase::CommitNotify ||
            phases[j] == TransactionPhase::AbortNotify) {
          is_in_sync = true;
        }
      }

      if (phases[i] == TransactionPhase::CommitNotify) {
        if (phases[j] == TransactionPhase::Commit ||
            phases[j] == TransactionPhase::CommitNotify ||
            phases[j] == TransactionPhase::AbortNotify ||
            phases[j] == TransactionPhase::Committed ||
            phases[j] == TransactionPhase::End) {
          is_in_sync = true;
        }
      }

      if (phases[i] == TransactionPhase::AbortNotify) {
        if (phases[j] == TransactionPhase::Commit ||
            phases[j] == TransactionPhase::CommitNotify ||
            phases[j] == TransactionPhase::AbortNotify ||
            phases[j] == TransactionPhase::Aborted ||
            phases[j] == TransactionPhase::End) {
          is_in_sync = true;
        }
      }

      if (phases[i] == TransactionPhase::Committed) {
        if (phases[j] == TransactionPhase::CommitNotify ||
            phases[j] == TransactionPhase::Committed ||
            phases[j] == TransactionPhase::End) {
          is_in_sync = true;
        }
      }

      if (phases[i] == TransactionPhase::Aborted) {
        if (phases[j] == TransactionPhase::AbortNotify ||
            phases[j] == TransactionPhase::Aborted ||
            phases[j] == TransactionPhase::End) {
          is_in_sync = true;
        }
      }

      if (phases[i] == TransactionPhase::End) {
        if (phases[j] == TransactionPhase::Committed ||
            phases[j] == TransactionPhase::Aborted ||
            phases[j] == TransactionPhase::End) {
          is_in_sync = true;
        }
      }

      EXPECT_EQ(mock_transaction_engine.LocalAndRemoteTransactionsInSync(
                    phases[i], phases[j]),
                is_in_sync);
    }
  }
}

TEST_F(TransactionEngineTest, ToTransactionExecutionPhase) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  vector<TransactionPhase> phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown,
  };

  vector<TransactionExecutionPhase> expected_phases = {
      TransactionExecutionPhase::Begin,   TransactionExecutionPhase::Begin,
      TransactionExecutionPhase::Prepare, TransactionExecutionPhase::Commit,
      TransactionExecutionPhase::Notify,  TransactionExecutionPhase::End,
      TransactionExecutionPhase::Abort,   TransactionExecutionPhase::End,
      TransactionExecutionPhase::End,     TransactionExecutionPhase::Unknown};

  for (size_t i = 0; i < phases.size(); ++i) {
    EXPECT_EQ(mock_transaction_engine.ToTransactionExecutionPhase(phases[i]),
              expected_phases[i]);
  }
}

TEST_F(TransactionEngineTest, CanCancel) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  vector<TransactionPhase> phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::Committed,
      TransactionPhase::AbortNotify,  TransactionPhase::Aborted,
      TransactionPhase::End,          TransactionPhase::Unknown,
  };

  for (size_t i = 0; i < phases.size(); ++i) {
    if (phases[i] == TransactionPhase::Committed ||
        phases[i] == TransactionPhase::AbortNotify ||
        phases[i] == TransactionPhase::Aborted ||
        phases[i] == TransactionPhase::End ||
        phases[i] == TransactionPhase::Unknown) {
      EXPECT_EQ(mock_transaction_engine.CanCancel(phases[i]), false);
    } else {
      EXPECT_EQ(mock_transaction_engine.CanCancel(phases[i]), true);
    }
  }
}

TEST_F(TransactionEngineTest, ExecutePhaseExpiredTransaction) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();

  EXPECT_THAT(mock_transaction_engine.ExecutePhase(transaction_phase_context),
              ResultIs(FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND)));

  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = false;
  transaction->id = transaction_id;
  transaction->transaction_secret = make_shared<string>("hidden secret");
  transaction->transaction_origin = make_shared<string>("hidden origin");

  auto pair = make_pair(transaction_id, transaction);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair, transaction),
            SuccessExecutionResult());

  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->transaction_secret =
      make_shared<string>("not matching");
  transaction_phase_context.request->transaction_origin =
      make_shared<string>("not matching");

  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY)));

  transaction->is_coordinated_remotely = true;
  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_SECRET_IS_NOT_VALID)));
}

TEST_F(TransactionEngineTest, ExecutePhase) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();

  EXPECT_THAT(mock_transaction_engine.ExecutePhase(transaction_phase_context),
              ResultIs(FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND)));

  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = false;
  transaction->id = transaction_id;
  transaction->transaction_secret = make_shared<string>("hidden secret");
  transaction->transaction_origin = make_shared<string>("hidden origin");
  auto pair = make_pair(transaction_id, transaction);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair, transaction),
            SuccessExecutionResult());

  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->transaction_secret =
      make_shared<string>("not matching");
  transaction_phase_context.request->transaction_origin =
      make_shared<string>("not matching");

  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY)));

  transaction->is_coordinated_remotely = true;
  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_SECRET_IS_NOT_VALID)));

  transaction->expiration_time = UINT64_MAX;
  transaction_phase_context.request->transaction_secret =
      transaction->transaction_secret;
  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_ORIGIN_IS_NOT_VALID)));

  transaction_phase_context.request->transaction_origin =
      transaction->transaction_origin;

  mock_transaction_engine.lock_remotely_coordinated_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        return FailureExecutionResult(12345);
      };

  EXPECT_THAT(
      mock_transaction_engine.ExecutePhase(transaction_phase_context),
      ResultIs(FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_CURRENT_TRANSACTION_IS_RUNNING)));

  mock_transaction_engine.lock_remotely_coordinated_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        return SuccessExecutionResult();
      };

  mock_transaction_engine.execute_phase_internal_mock =
      [&](shared_ptr<Transaction>& transaction,
          AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) { return SuccessExecutionResult(); };

  EXPECT_SUCCESS(
      mock_transaction_engine.ExecutePhase(transaction_phase_context));

  bool called = false;
  mock_transaction_engine.unlock_remotely_coordinated_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) {
        called = true;
        return SuccessExecutionResult();
      };

  mock_transaction_engine.execute_phase_internal_mock =
      [&](shared_ptr<Transaction>& transaction,
          AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        return FailureExecutionResult(12345);
      };

  EXPECT_THAT(mock_transaction_engine.ExecutePhase(transaction_phase_context),
              ResultIs(FailureExecutionResult(12345)));
  EXPECT_EQ(called, true);
}

TEST_F(TransactionEngineTest, ExecutePhaseInternal) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();

  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = false;
  transaction->id = transaction_id;
  transaction->transaction_secret = make_shared<string>("hidden secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->last_execution_timestamp = 99;

  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            FailureExecutionResult(
                errors::SC_TRANSACTION_MANAGER_TRANSACTION_IS_NOT_LOCKED));

  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = true;
  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            FailureExecutionResult(
                errors::SC_TRANSACTION_MANAGER_TRANSACTION_IS_NOT_LOCKED));

  transaction->is_waiting_for_remote = false;
  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            FailureExecutionResult(
                errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND));

  auto pair = make_pair(transaction_id, transaction);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair, transaction),
            SuccessExecutionResult());

  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->transaction_secret =
      make_shared<string>("not matching");
  transaction_phase_context.request->transaction_origin =
      make_shared<string>("not matching origin");
  transaction_phase_context.request->last_execution_timestamp = 1234;

  EXPECT_EQ(
      mock_transaction_engine.ExecutePhaseInternal(transaction,
                                                   transaction_phase_context),
      FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_LAST_EXECUTION_TIMESTAMP_NOT_MATCHING));
}

TEST_F(TransactionEngineTest, ExecutePhaseInternalHandlePhases) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();

  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->id = transaction_id;
  transaction->last_execution_timestamp = 99;

  auto pair = make_pair(transaction_id, transaction);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair, transaction),
            SuccessExecutionResult());

  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->last_execution_timestamp =
      transaction->last_execution_timestamp;

  vector<TransactionExecutionPhase> requested_phases = {
      TransactionExecutionPhase::Begin,   TransactionExecutionPhase::Prepare,
      TransactionExecutionPhase::Commit,  TransactionExecutionPhase::Notify,
      TransactionExecutionPhase::Abort,   TransactionExecutionPhase::End,
      TransactionExecutionPhase::Unknown,
  };

  vector<TransactionPhase> all_phases = {
      TransactionPhase::NotStarted,   TransactionPhase::Begin,
      TransactionPhase::Prepare,      TransactionPhase::Commit,
      TransactionPhase::CommitNotify, TransactionPhase::AbortNotify,
      TransactionPhase::Committed,    TransactionPhase::Aborted,
      TransactionPhase::Unknown,
  };

  map<TransactionExecutionPhase, set<TransactionPhase>> valid_phases = {
      {TransactionExecutionPhase::Begin, {TransactionPhase::Begin}},
      {TransactionExecutionPhase::Prepare, {TransactionPhase::Prepare}},
      {TransactionExecutionPhase::Commit, {TransactionPhase::Commit}},
      {TransactionExecutionPhase::Notify, {TransactionPhase::CommitNotify}},
      {TransactionExecutionPhase::Abort,
       {TransactionPhase::Begin, TransactionPhase::Prepare,
        TransactionPhase::Commit, TransactionPhase::CommitNotify,
        TransactionPhase::AbortNotify}},
      {TransactionExecutionPhase::End,
       {TransactionPhase::Begin, TransactionPhase::Prepare,
        TransactionPhase::Aborted, TransactionPhase::Committed,
        TransactionPhase::End}}};

  for (auto requested_phase : requested_phases) {
    transaction_phase_context.request->transaction_execution_phase =
        requested_phase;

    for (auto phase : all_phases) {
      if (valid_phases[requested_phase].find(phase) !=
          valid_phases[requested_phase].end()) {
        continue;
      }

      transaction->current_phase = phase;
      EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                    transaction, transaction_phase_context),
                FailureExecutionResult(
                    errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE));
    }
  }
}

TEST_F(TransactionEngineTest, ExecutePhaseInternalHandlePhasesWithCallback) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();

  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->id = transaction_id;
  transaction->last_execution_timestamp = 99;

  auto pair = make_pair(transaction_id, transaction);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair, transaction),
            SuccessExecutionResult());

  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->last_execution_timestamp =
      transaction->last_execution_timestamp;

  transaction->current_phase = TransactionPhase::Begin;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Begin;
  atomic<bool> called = false;
  mock_transaction_engine.begin_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) { called = true; };

  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });

  transaction->current_phase = TransactionPhase::Prepare;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Prepare;
  called = false;
  mock_transaction_engine.prepare_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) { called = true; };

  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });

  transaction->current_phase = TransactionPhase::Commit;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Commit;
  called = false;
  mock_transaction_engine.commit_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) { called = true; };

  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });

  transaction->current_phase = TransactionPhase::CommitNotify;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Notify;
  called = false;
  mock_transaction_engine.commit_notify_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) { called = true; };

  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });

  transaction->current_phase = TransactionPhase::AbortNotify;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Abort;
  called = false;
  mock_transaction_engine.abort_notify_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) { called = true; };

  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });
  EXPECT_EQ(transaction->current_phase, TransactionPhase::AbortNotify);
  EXPECT_EQ(transaction->transaction_failed.load(), true);
  EXPECT_THAT(transaction->transaction_execution_result,
              ResultIs(FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_TRANSACTION_IS_ABORTED)));
  EXPECT_EQ(transaction->current_phase_failed.load(), false);
  EXPECT_SUCCESS(transaction->current_phase_execution_result);

  transaction->current_phase = TransactionPhase::Aborted;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::End;
  called = false;
  mock_transaction_engine.end_transaction_mock =
      [&](shared_ptr<Transaction>& transaction) { called = true; };

  EXPECT_EQ(mock_transaction_engine.ExecutePhaseInternal(
                transaction, transaction_phase_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });
  EXPECT_EQ(transaction->current_phase, TransactionPhase::End);
}

TEST_F(TransactionEngineTest, GetTransactionStatus) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = false;
  transaction->is_waiting_for_remote = false;
  transaction->id = transaction_id;
  transaction->last_execution_timestamp = 99;
  transaction->transaction_secret = make_shared<string>("txn-secret");

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.request =
      make_shared<GetTransactionStatusRequest>();
  get_transaction_status_context.request->transaction_id = transaction_id;
  get_transaction_status_context.request->transaction_secret =
      make_shared<string>("txn-secret");

  EXPECT_EQ(mock_transaction_engine.GetTransactionStatus(
                get_transaction_status_context),
            FailureExecutionResult(
                errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND));

  auto pair = make_pair(transaction_id, transaction);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair, transaction),
            SuccessExecutionResult());

  EXPECT_EQ(
      mock_transaction_engine.GetTransactionStatus(
          get_transaction_status_context),
      FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY));

  transaction->is_coordinated_remotely = true;

  atomic<bool> called = false;
  get_transaction_status_context
      .callback = [&](AsyncContext<GetTransactionStatusRequest,
                                   GetTransactionStatusResponse>&
                          get_transaction_status_context) {
    EXPECT_SUCCESS(get_transaction_status_context.result);
    EXPECT_EQ(get_transaction_status_context.response->has_failure, false);
    EXPECT_EQ(
        get_transaction_status_context.response->transaction_execution_phase,
        mock_transaction_engine.ToTransactionExecutionPhase(
            transaction->current_phase.load()));
    EXPECT_EQ(get_transaction_status_context.response->last_execution_timestamp,
              transaction->last_execution_timestamp);
    EXPECT_EQ(get_transaction_status_context.response->is_expired,
              transaction->IsExpired());
    called = true;
  };

  EXPECT_EQ(mock_transaction_engine.GetTransactionStatus(
                get_transaction_status_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });

  called = false;
  transaction->current_phase_failed = true;
  get_transaction_status_context.callback =
      [&](AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>&
              get_transaction_status_context) {
        EXPECT_EQ(get_transaction_status_context.response->has_failure, true);
        called = true;
      };

  EXPECT_EQ(mock_transaction_engine.GetTransactionStatus(
                get_transaction_status_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });

  called = false;
  transaction->transaction_failed = true;
  get_transaction_status_context.callback =
      [&](AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>&
              get_transaction_status_context) {
        EXPECT_EQ(get_transaction_status_context.response->has_failure, true);
        called = true;
      };

  EXPECT_EQ(mock_transaction_engine.GetTransactionStatus(
                get_transaction_status_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest,
       GetTransactionStatusFailsWhenIncorrectTransactionSecretIsSupplied) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_waiting_for_remote = false;
  transaction->id = transaction_id;
  transaction->last_execution_timestamp = 99;
  transaction->transaction_secret = make_shared<string>("correct-txn-secret");
  transaction->is_coordinated_remotely = true;

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.request =
      make_shared<GetTransactionStatusRequest>();
  get_transaction_status_context.request->transaction_id = transaction_id;

  auto pair = make_pair(transaction_id, transaction);
  EXPECT_EQ(mock_transaction_engine.GetActiveTransactionsMap().Insert(
                pair, transaction),
            SuccessExecutionResult());

  // No transaction secret supplied
  EXPECT_EQ(
      mock_transaction_engine.GetTransactionStatus(
          get_transaction_status_context),
      FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_SECRET_IS_NOT_VALID));

  // Incorrect transaction secret supplied
  get_transaction_status_context.request->transaction_secret =
      make_shared<string>("another-txn-secret");
  EXPECT_EQ(
      mock_transaction_engine.GetTransactionStatus(
          get_transaction_status_context),
      FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_SECRET_IS_NOT_VALID));

  // Correct transaction secret supplied
  get_transaction_status_context.request->transaction_secret =
      make_shared<string>("correct-txn-secret");

  atomic<bool> called = false;
  get_transaction_status_context
      .callback = [&](AsyncContext<GetTransactionStatusRequest,
                                   GetTransactionStatusResponse>&
                          get_transaction_status_context) {
    EXPECT_SUCCESS(get_transaction_status_context.result);
    EXPECT_EQ(get_transaction_status_context.response->has_failure, false);
    EXPECT_EQ(
        get_transaction_status_context.response->transaction_execution_phase,
        mock_transaction_engine.ToTransactionExecutionPhase(
            transaction->current_phase.load()));
    EXPECT_EQ(get_transaction_status_context.response->last_execution_timestamp,
              transaction->last_execution_timestamp);
    EXPECT_EQ(get_transaction_status_context.response->is_expired,
              transaction->IsExpired());
    called = true;
  };

  EXPECT_EQ(mock_transaction_engine.GetTransactionStatus(
                get_transaction_status_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return called.load(); });
}

TEST_F(TransactionEngineTest, OnPhaseCallbackWithErrors) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);
  auto current_phase = TransactionPhase::Commit;

  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = false;
  transaction->is_waiting_for_remote = false;
  transaction->last_execution_timestamp = 99;

  for (auto i = 0; i < 100; ++i) {
    auto execution_result = FailureExecutionResult(1234);
    mock_transaction_engine.OnPhaseCallback(i, current_phase, execution_result,
                                            transaction);

    size_t command_index;
    EXPECT_EQ(transaction->current_phase_failed_command_indices.TryDequeue(
                  command_index),
              SuccessExecutionResult());

    EXPECT_EQ(command_index, i);
  }
}

TEST_F(TransactionEngineTest, ProceedToNextPhaseToGetFailedIndices) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  struct IndexedTransactionCommand : public TransactionCommand {
    explicit IndexedTransactionCommand(int index) : index_(index) {}

    int index_;
  };

  transaction_manager::TransactionPhase current_phase = TransactionPhase::Begin;
  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  bool called = false;
  transaction->current_phase = TransactionPhase::Begin;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->id = transaction_id;
  transaction->last_execution_timestamp = 99;
  transaction->current_phase_failed = true;
  transaction->current_phase_execution_result = FailureExecutionResult(123);
  // Push 100 IndexedTransactionCommand commands.
  transaction->context.request = make_shared<TransactionRequest>();
  for (int i = 0; i < 100; i++) {
    transaction->context.request->commands.push_back(
        make_shared<IndexedTransactionCommand>(i));
  }
  transaction->remote_phase_context.request =
      make_shared<TransactionPhaseRequest>();
  transaction->remote_phase_context.callback =
      [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              context) mutable {
        EXPECT_EQ(context.response->failed_commands_indices.size(), 50);
        EXPECT_EQ(context.response->failed_commands.size(), 50);
        // Verify only commands with even index have failed
        int index = 0;
        for (auto& failed_command : context.response->failed_commands) {
          auto command =
              dynamic_pointer_cast<IndexedTransactionCommand>(failed_command);
          EXPECT_EQ(command->index_, index);
          index += 2;
        }
        called = true;
      };

  for (size_t i = 0; i < 100; ++i) {
    if (i % 2 == 0) {
      transaction->current_phase_failed_command_indices.TryEnqueue(i);
    }
  }

  mock_transaction_engine.ProceedToNextPhase(current_phase, transaction);
  EXPECT_EQ(called, true);
}

TEST_F(TransactionEngineTest, ExecuteDistributedPhaseWithDispatchError) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<TransactionPhaseManagerInterface> transaction_phase_manager =
      make_shared<TransactionPhaseManager>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  MockTransactionEngine mock_transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client);

  transaction_manager::TransactionPhase current_phase =
      TransactionPhase::Prepare;
  Uuid transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Prepare;
  transaction->expiration_time = 0;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = false;
  transaction->id = transaction_id;
  transaction->last_execution_timestamp = 99;
  transaction->context.request = make_shared<TransactionRequest>();

  for (size_t i = 0; i < 100; ++i) {
    TransactionCommand command;
    command.prepare = [index = i](TransactionCommandCallback&) mutable {
      ExecutionResult result;
      if (index % 2 == 0) {
        result = FailureExecutionResult(123);
        return result;
      }
      result = SuccessExecutionResult();
      return result;
    };

    transaction->context.request->commands.push_back(
        make_shared<TransactionCommand>(command));
  }

  EXPECT_EQ(transaction->current_phase_failed_command_indices.Size(), 0);
  mock_transaction_engine.ExecuteDistributedPhase(current_phase, transaction);
  EXPECT_EQ(transaction->current_phase_failed_command_indices.Size(), 50);
}

TEST_F(TransactionEngineTest,
       GetPendingTransactionCountReturnsZeroWhenNoTransactions) {
  EXPECT_SUCCESS(mock_transaction_engine_->Init());
  EXPECT_EQ(mock_transaction_engine_->GetPendingTransactionCount(), 0);
}

TEST_F(TransactionEngineTest,
       GetPendingTransactionCountReturnsNonZeroWhenThereAreTransactions) {
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();

  mock_transaction_engine_->Execute(transaction_context);

  EXPECT_EQ(mock_transaction_engine_->GetPendingTransactionCount(), 1);
}

TEST_F(TransactionEngineTest, TransactionPhaseToString) {
  EXPECT_EQ("NOT_STARTED",
            MockTransactionEngine::TransactionPhaseToString(
                transaction_manager::TransactionPhase::NotStarted));

  EXPECT_EQ("BEGIN", MockTransactionEngine::TransactionPhaseToString(
                         transaction_manager::TransactionPhase::Begin));

  EXPECT_EQ("PREPARE", MockTransactionEngine::TransactionPhaseToString(
                           transaction_manager::TransactionPhase::Prepare));

  EXPECT_EQ("COMMIT", MockTransactionEngine::TransactionPhaseToString(
                          transaction_manager::TransactionPhase::Commit));

  EXPECT_EQ("COMMIT_NOTIFY",
            MockTransactionEngine::TransactionPhaseToString(
                transaction_manager::TransactionPhase::CommitNotify));

  EXPECT_EQ("COMMITTED", MockTransactionEngine::TransactionPhaseToString(
                             transaction_manager::TransactionPhase::Committed));

  EXPECT_EQ("ABORT_NOTIFY",
            MockTransactionEngine::TransactionPhaseToString(
                transaction_manager::TransactionPhase::AbortNotify));

  EXPECT_EQ("ABORTED", MockTransactionEngine::TransactionPhaseToString(
                           transaction_manager::TransactionPhase::Aborted));

  EXPECT_EQ("END", MockTransactionEngine::TransactionPhaseToString(
                       transaction_manager::TransactionPhase::End));

  EXPECT_EQ("UNKNOWN", MockTransactionEngine::TransactionPhaseToString(
                           transaction_manager::TransactionPhase::Unknown));

  // Next free value.
  EXPECT_EQ(
      "UNKNOWN",
      MockTransactionEngine::TransactionPhaseToString(
          static_cast<transaction_manager::TransactionPhase>(
              static_cast<int>(transaction_manager::TransactionPhase::End) +
              1)));
}

TEST_F(TransactionEngineTest, TransactionExecutionPhaseToString) {
  EXPECT_EQ("BEGIN", MockTransactionEngine::TransactionExecutionPhaseToString(
                         TransactionExecutionPhase::Begin));

  EXPECT_EQ("PREPARE", MockTransactionEngine::TransactionExecutionPhaseToString(
                           TransactionExecutionPhase::Prepare));

  EXPECT_EQ("COMMIT", MockTransactionEngine::TransactionExecutionPhaseToString(
                          TransactionExecutionPhase::Commit));

  EXPECT_EQ("COMMIT_NOTIFY",
            MockTransactionEngine::TransactionExecutionPhaseToString(
                TransactionExecutionPhase::Notify));

  EXPECT_EQ("ABORT_NOTIFY",
            MockTransactionEngine::TransactionExecutionPhaseToString(
                TransactionExecutionPhase::Abort));

  EXPECT_EQ("END", MockTransactionEngine::TransactionExecutionPhaseToString(
                       TransactionExecutionPhase::End));

  EXPECT_EQ("UNKNOWN", MockTransactionEngine::TransactionExecutionPhaseToString(
                           TransactionExecutionPhase::Unknown));

  // Next free value.
  EXPECT_EQ("UNKNOWN",
            MockTransactionEngine::TransactionExecutionPhaseToString(
                static_cast<TransactionExecutionPhase>(
                    static_cast<int>(TransactionExecutionPhase::End) + 1)));
}

TEST_F(TransactionEngineTest,
       StopWaitsUntilTransactionsNotWaitingForRemoteComplete) {
  auto transaction_id = Uuid::GenerateUuid();
  auto transaction1 = make_shared<Transaction>();
  transaction1->current_phase = TransactionPhase::Prepare;
  transaction1->is_coordinated_remotely = true;
  transaction1->context.request = make_shared<TransactionRequest>();
  auto pair = make_pair(transaction_id, transaction1);
  mock_transaction_engine_->GetActiveTransactionsMap().Insert(pair,
                                                              transaction1);

  transaction_id = Uuid::GenerateUuid();
  auto transaction2 = make_shared<Transaction>();
  transaction2->current_phase = TransactionPhase::Prepare;
  transaction2->is_coordinated_remotely = true;
  transaction2->context.request = make_shared<TransactionRequest>();
  pair = make_pair(transaction_id, transaction2);
  mock_transaction_engine_->GetActiveTransactionsMap().Insert(pair,
                                                              transaction2);

  mock_transaction_engine_->prepare_transaction_mock =
      [](shared_ptr<Transaction>& transaction) {
        AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse> context(
            make_shared<TransactionPhaseRequest>(),
            [](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                   context) {});
        transaction->remote_phase_context.callback(context);
      };
  mock_async_executor_->schedule_mock = [&](const auto& work) {
    work();
    return SuccessExecutionResult();
  };
  mock_async_executor_->schedule_for_mock = [&](const auto& a, auto timestmap,
                                                auto& cancellation_callback) {
    cancellation_callback = []() { return true; };
    return SuccessExecutionResult();
  };

  EXPECT_SUCCESS(mock_async_executor_->Init());
  EXPECT_SUCCESS(mock_async_executor_->Run());
  EXPECT_SUCCESS(mock_transaction_engine_->Run());

  std::atomic<bool> started_stopping = false;
  std::atomic<bool> stopped = false;
  auto thread = std::thread([&]() {
    started_stopping = true;
    mock_transaction_engine_->Stop();
    stopped = true;
  });

  WaitUntil([&]() { return started_stopping.load(); });
  EXPECT_THROW(
      WaitUntil([&]() { return stopped.load(); }, std::chrono::seconds(1)),
      TestTimeoutException);

  // First transaction finishes.
  transaction1->is_waiting_for_remote = true;

  EXPECT_THROW(
      WaitUntil([&]() { return stopped.load(); }, std::chrono::seconds(1)),
      TestTimeoutException);

  // Second transaction finishes.
  transaction2->is_waiting_for_remote = true;

  WaitUntil([&]() { return stopped.load(); });
  thread.join();

  EXPECT_SUCCESS(mock_async_executor_->Stop());
}

TEST_F(TransactionEngineTest,
       ResolveTransactionIsNoOperationWhenRemoteResolutionDisabled) {
  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      shared_ptr<MockAsyncExecutor>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();

  mock_config_provider->SetBool(kTransactionResolutionWithRemoteEnabled, false);

  MockTransactionEngine transaction_engine(
      async_executor, mock_transaction_command_serializer, journal_service,
      remote_transaction_manager, mock_metric_client, mock_config_provider);

  EXPECT_SUCCESS(transaction_engine.Init());

  auto transaction = make_shared<Transaction>();
  transaction->id = Uuid::GenerateUuid();
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = true;
  transaction->transaction_secret = make_shared<string>("this is the secret");
  transaction->transaction_origin = make_shared<string>("origin.com");
  transaction->expiration_time = 0;

  EXPECT_SUCCESS(transaction_engine.ResolveTransaction(transaction));
}

}  // namespace google::scp::core::test
