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
#include <list>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/common/uuid/src/uuid.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/test/utils/conditional_wait.h"
#include "core/transaction_manager/mock/mock_transaction_command_serializer.h"
#include "core/transaction_manager/mock/mock_transaction_engine.h"
#include "core/transaction_manager/mock/mock_transaction_manager.h"
#include "core/transaction_manager/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncOperation;
using google::scp::core::CheckpointLog;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RemoteTransactionManagerInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::test::WaitUntil;
using google::scp::core::transaction_manager::TransactionEngineInterface;
using google::scp::core::transaction_manager::mock::
    MockTransactionCommandSerializer;
using google::scp::core::transaction_manager::mock::MockTransactionEngine;
using google::scp::core::transaction_manager::mock::MockTransactionManager;
using google::scp::cpio::MockMetricClient;
using std::atomic;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::static_pointer_cast;
using std::thread;
using std::vector;

namespace google::scp::core::test {

class TransactionManagerTests : public testing::Test {
 protected:
  void CreateComponents() {
    shared_ptr<JournalServiceInterface> mock_journal_service;
    shared_ptr<TransactionCommandSerializerInterface>
        mock_transaction_command_serializer;
    shared_ptr<AsyncExecutorInterface> async_executor;
    shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;

    mock_journal_service = make_shared<MockJournalService>();
    mock_transaction_command_serializer =
        make_shared<MockTransactionCommandSerializer>();
    async_executor = make_shared<MockAsyncExecutor>();
    auto metric_client = make_shared<MockMetricClient>();
    mock_transaction_engine_ = make_shared<MockTransactionEngine>(
        async_executor, mock_transaction_command_serializer,
        mock_journal_service, remote_transaction_manager, metric_client);
    mock_transaction_manager_ = make_shared<MockTransactionManager>(
        async_executor, mock_transaction_engine_, 100000, metric_client);
  }

  TransactionManagerTests() { CreateComponents(); }

  shared_ptr<MockTransactionEngine> mock_transaction_engine_;
  shared_ptr<MockTransactionManager> mock_transaction_manager_;
};

TEST_F(TransactionManagerTests, InitValidation) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto mock_metric_client = make_shared<MockMetricClient>();
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_transaction_engine = make_shared<MockTransactionEngine>(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  mock_transaction_engine->init_mock = []() {
    return SuccessExecutionResult();
  };
  mock_transaction_engine->run_mock = []() { return SuccessExecutionResult(); };
  mock_transaction_engine->stop_mock = []() {
    return SuccessExecutionResult();
  };
  auto transaction_engine =
      static_pointer_cast<TransactionEngineInterface>(mock_transaction_engine);

  {
    MockTransactionManager transaction_manager(
        async_executor, transaction_engine, 0, mock_metric_client);
    auto code = errors::
        SC_TRANSACTION_MANAGER_INVALID_MAX_CONCURRENT_TRANSACTIONS_VALUE;
    EXPECT_THAT(transaction_manager.Init(),
                ResultIs(FailureExecutionResult(code)));
  }

  {
    auto mock_async_executor = make_shared<MockAsyncExecutor>();
    MockTransactionManager transaction_manager(
        mock_async_executor, transaction_engine, 1, mock_metric_client);
    mock_async_executor->schedule_mock = [](const AsyncOperation&) {
      return SuccessExecutionResult();
    };
    EXPECT_SUCCESS(transaction_manager.Init());
    EXPECT_SUCCESS(transaction_manager.Run());
    EXPECT_THAT(transaction_manager.Init(),
                ResultIs(FailureExecutionResult(
                    errors::SC_TRANSACTION_MANAGER_ALREADY_STARTED)));
    EXPECT_SUCCESS(transaction_manager.Stop());
  }
}

TEST_F(TransactionManagerTests, RunValidation) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_transaction_engine = make_shared<MockTransactionEngine>(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  mock_transaction_engine->init_mock = []() {
    return SuccessExecutionResult();
  };
  mock_transaction_engine->run_mock = []() { return SuccessExecutionResult(); };
  mock_transaction_engine->stop_mock = []() {
    return SuccessExecutionResult();
  };
  auto transaction_engine =
      static_pointer_cast<TransactionEngineInterface>(mock_transaction_engine);

  {
    MockTransactionManager transaction_manager(
        async_executor, transaction_engine, 1, mock_metric_client);
    mock_async_executor->schedule_mock = [](const AsyncOperation&) {
      return SuccessExecutionResult();
    };
    EXPECT_SUCCESS(transaction_manager.Init());
    EXPECT_SUCCESS(transaction_manager.Run());
    EXPECT_THAT(transaction_manager.Run(),
                ResultIs(FailureExecutionResult(
                    errors::SC_TRANSACTION_MANAGER_ALREADY_STARTED)));
    EXPECT_SUCCESS(transaction_manager.Stop());
  }
}

TEST_F(TransactionManagerTests, ExecuteValidation) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  {
    auto mock_async_executor = make_shared<MockAsyncExecutor>();
    auto async_executor =
        static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
    shared_ptr<JournalServiceInterface> mock_journal_service =
        make_shared<MockJournalService>();
    shared_ptr<TransactionCommandSerializerInterface>
        mock_transaction_command_serializer =
            make_shared<MockTransactionCommandSerializer>();

    shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
    auto mock_transaction_engine = make_shared<MockTransactionEngine>(
        async_executor, mock_transaction_command_serializer,
        mock_journal_service, remote_transaction_manager, mock_metric_client);

    mock_transaction_engine->init_mock = []() {
      return SuccessExecutionResult();
    };
    mock_transaction_engine->run_mock = []() {
      return SuccessExecutionResult();
    };
    mock_transaction_engine->stop_mock = []() {
      return SuccessExecutionResult();
    };
    auto transaction_engine = static_pointer_cast<TransactionEngineInterface>(
        mock_transaction_engine);

    MockTransactionManager transaction_manager(
        async_executor, transaction_engine, 1, mock_metric_client);
    AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
    transaction_context.request = make_shared<TransactionRequest>();
    transaction_context.request->transaction_id = Uuid::GenerateUuid();
    EXPECT_THAT(transaction_manager.Execute(transaction_context),
                ResultIs(FailureExecutionResult(
                    errors::SC_TRANSACTION_MANAGER_NOT_STARTED)));
  }

  {
    auto mock_async_executor = make_shared<MockAsyncExecutor>();
    auto async_executor =
        static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
    shared_ptr<JournalServiceInterface> mock_journal_service =
        make_shared<MockJournalService>();
    shared_ptr<TransactionCommandSerializerInterface>
        mock_transaction_command_serializer =
            make_shared<MockTransactionCommandSerializer>();
    shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
    auto mock_transaction_engine = make_shared<MockTransactionEngine>(
        async_executor, mock_transaction_command_serializer,
        mock_journal_service, remote_transaction_manager, mock_metric_client);

    shared_ptr<TransactionEngineInterface> transaction_engine =
        mock_transaction_engine;
    mock_transaction_engine->init_mock = []() {
      return SuccessExecutionResult();
    };
    mock_transaction_engine->run_mock = []() {
      return SuccessExecutionResult();
    };
    mock_transaction_engine->stop_mock = []() {
      return SuccessExecutionResult();
    };
    MockTransactionManager transaction_manager(
        mock_async_executor, transaction_engine, 1, mock_metric_client);
    EXPECT_SUCCESS(transaction_manager.Init());
    EXPECT_SUCCESS(transaction_manager.Run());
    AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
    transaction_context.request = make_shared<TransactionRequest>();
    transaction_context.request->transaction_id = Uuid::GenerateUuid();
    transaction_manager.GetActiveTransactionsCount()++;
    transaction_manager.GetActiveTransactionsCount()++;
    EXPECT_THAT(
        transaction_manager.Execute(transaction_context),
        ResultIs(RetryExecutionResult(
            errors::SC_TRANSACTION_MANAGER_CANNOT_ACCEPT_NEW_REQUESTS)));

    transaction_manager.GetActiveTransactionsCount()--;
    transaction_manager.GetActiveTransactionsCount()--;
    EXPECT_SUCCESS(transaction_manager.Stop());
  }

  {
    auto mock_async_executor = make_shared<MockAsyncExecutor>();
    auto async_executor =
        static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
    shared_ptr<JournalServiceInterface> mock_journal_service =
        make_shared<MockJournalService>();
    shared_ptr<TransactionCommandSerializerInterface>
        mock_transaction_command_serializer =
            make_shared<MockTransactionCommandSerializer>();
    shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
    auto mock_transaction_engine = make_shared<MockTransactionEngine>(
        async_executor, mock_transaction_command_serializer,
        mock_journal_service, remote_transaction_manager, mock_metric_client);

    shared_ptr<TransactionEngineInterface> transaction_engine =
        mock_transaction_engine;
    mock_transaction_engine->init_mock = []() {
      return SuccessExecutionResult();
    };
    mock_transaction_engine->run_mock = []() {
      return SuccessExecutionResult();
    };
    mock_transaction_engine->stop_mock = []() {
      return SuccessExecutionResult();
    };
    MockTransactionManager transaction_manager(
        mock_async_executor, mock_transaction_engine, 1000, mock_metric_client);

    mock_transaction_engine->log_transaction_and_proceed_to_next_phase_mock =
        [](auto next_phase, auto transaction) {
          transaction->context.result = SuccessExecutionResult();
          transaction->context.Finish();
          return SuccessExecutionResult();
        };

    atomic<size_t> total = 0;
    vector<thread> threads;
    mock_async_executor->schedule_mock = [&](const AsyncOperation& work) {
      threads.push_back(thread([work]() { work(); }));
      return SuccessExecutionResult();
    };

    EXPECT_SUCCESS(transaction_manager.Init());
    EXPECT_SUCCESS(transaction_manager.Run());

    for (size_t i = 0; i < 5; ++i) {
      AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
      transaction_context.request = make_shared<TransactionRequest>();
      transaction_context.request->transaction_id = Uuid::GenerateUuid();
      transaction_context.callback = [&](auto& context) { total++; };
      EXPECT_SUCCESS(transaction_manager.Execute(transaction_context));
    }

    WaitUntil([&]() { return total.load() == 5; });
    EXPECT_SUCCESS(transaction_manager.Stop());
    for (size_t i = 0; i < threads.size(); ++i) {
      if (threads[i].joinable()) {
        threads[i].join();
      }
    }
  }
}

TEST_F(TransactionManagerTests, StopValidation) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_transaction_engine = make_shared<MockTransactionEngine>(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<TransactionEngineInterface> transaction_engine =
      mock_transaction_engine;
  mock_transaction_engine->init_mock = []() {
    return SuccessExecutionResult();
  };
  mock_transaction_engine->run_mock = []() { return SuccessExecutionResult(); };
  mock_transaction_engine->stop_mock = []() {
    return SuccessExecutionResult();
  };

  {
    MockTransactionManager transaction_manager(
        async_executor, transaction_engine, 1, mock_metric_client);
    EXPECT_THAT(transaction_manager.Stop(),
                ResultIs(FailureExecutionResult(
                    errors::SC_TRANSACTION_MANAGER_ALREADY_STOPPED)));
  }

  {
    auto mock_async_executor = make_shared<MockAsyncExecutor>();
    MockTransactionManager transaction_manager(
        mock_async_executor, transaction_engine, 1, mock_metric_client);

    EXPECT_SUCCESS(transaction_manager.Init());
    EXPECT_SUCCESS(transaction_manager.Run());

    transaction_manager.GetActiveTransactionsCount()++;
    transaction_manager.GetActiveTransactionsCount()++;

    atomic<bool> finished = false;
    thread decrement_active_transactions([&transaction_manager, &finished]() {
      transaction_manager.GetActiveTransactionsCount() -= 2;
      finished = true;
    });

    EXPECT_SUCCESS(transaction_manager.Stop());
    EXPECT_TRUE(finished.load());

    if (decrement_active_transactions.joinable()) {
      decrement_active_transactions.join();
    }
  }
}

TEST_F(TransactionManagerTests, CannotCheckpointIfRunning) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_transaction_engine = make_shared<MockTransactionEngine>(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client);
  shared_ptr<TransactionEngineInterface> transaction_engine =
      mock_transaction_engine;
  mock_transaction_engine->init_mock = []() {
    return SuccessExecutionResult();
  };
  mock_transaction_engine->run_mock = []() { return SuccessExecutionResult(); };
  mock_transaction_engine->stop_mock = []() {
    return SuccessExecutionResult();
  };

  MockTransactionManager transaction_manager(async_executor, transaction_engine,
                                             1, mock_metric_client);
  transaction_manager.Init();
  auto checkpoint_logs = make_shared<list<CheckpointLog>>();
  EXPECT_SUCCESS(transaction_manager.Checkpoint(checkpoint_logs));

  transaction_manager.Run();
  EXPECT_THAT(
      transaction_manager.Checkpoint(checkpoint_logs),
      ResultIs(FailureExecutionResult(
          errors::
              SC_TRANSACTION_MANAGER_CANNOT_CREATE_CHECKPOINT_WHEN_STARTED)));

  transaction_manager.Stop();
}

TEST_F(TransactionManagerTests,
       GetStatusReturnsFailureIfTransactionManagerHasNotStarted) {
  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;
  EXPECT_THAT(
      mock_transaction_manager_->GetTransactionManagerStatus(request, response),
      ResultIs(FailureExecutionResult(
          core::errors::SC_TRANSACTION_MANAGER_STATUS_CANNOT_BE_OBTAINED)));
}

TEST_F(TransactionManagerTests, GetStatusReturnsZeroPendingTransactionsCount) {
  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;

  mock_transaction_engine_->init_mock = []() {
    return SuccessExecutionResult();
  };
  mock_transaction_engine_->run_mock = []() {
    return SuccessExecutionResult();
  };

  EXPECT_SUCCESS(mock_transaction_manager_->Init());
  EXPECT_SUCCESS(mock_transaction_manager_->Run());

  EXPECT_SUCCESS(mock_transaction_manager_->GetTransactionManagerStatus(
      request, response));
  EXPECT_EQ(response.pending_transactions_count, 0);
}

TEST_F(TransactionManagerTests,
       GetStatusReturnsNonZeroPendingTransactionsCount) {
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;

  mock_transaction_engine_->init_mock = []() {
    return SuccessExecutionResult();
  };
  mock_transaction_engine_->run_mock = []() {
    return SuccessExecutionResult();
  };

  EXPECT_SUCCESS(mock_transaction_manager_->Init());
  EXPECT_SUCCESS(mock_transaction_manager_->Run());

  // Add 1 transaction
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  mock_transaction_engine_->Execute(transaction_context);

  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;
  EXPECT_SUCCESS(mock_transaction_manager_->GetTransactionManagerStatus(
      request, response));
  EXPECT_EQ(response.pending_transactions_count, 1);
}

}  // namespace google::scp::core::test
