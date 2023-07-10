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
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <aws/core/Aws.h>

#include "core/async_executor/src/async_executor.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/error_codes.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/errors.h"
#include "core/interface/token_provider_cache_interface.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/test/utils/conditional_wait.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "pbs/pbs_client/src/transactional/pbs_transactional_client.h"
#include "pbs/test/e2e/test_pbs_server_starter/test_pbs_server_starter.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::LoggerInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::Uuid;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT;
using google::scp::core::errors::SC_HTTP2_CLIENT_NOT_OK_RESPONSE;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::pbs::PrivacyBudgetServiceTransactionalClient;
using google::scp::pbs::test::e2e::AuthResult;
using google::scp::pbs::test::e2e::TestPbsConfig;
using google::scp::pbs::test::e2e::TestPbsDataConfig;
using google::scp::pbs::test::e2e::TestPbsServerStarter;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::mt19937;
using std::random_device;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

static constexpr char kRegion[] = "us-east-1";
static constexpr char kLocalHost[] = "http://127.0.0.1";
static constexpr char kReportnigOrigin[] = "test.com";
// TODO(b/241857324): pick available ports randomly.
static constexpr char kLocalstackPort[] = "4566";
// static const Uuid kTransactionId = Uuid::GenerateUuid();
static constexpr char kTransactionSecret[] = "transaction_secret";
std::vector<TransactionExecutionPhase> kTransactionPhases(
    {TransactionExecutionPhase::Prepare, TransactionExecutionPhase::Commit,
     TransactionExecutionPhase::Notify});

namespace google::scp::pbs::test::e2e {
static string GetRandomString(const string& prefix) {
  // Bucket name can only be lower case.
  string str("abcdefghijklmnopqrstuvwxyz");
  static random_device random_device_local;
  static mt19937 generator(random_device_local());
  shuffle(str.begin(), str.end(), generator);
  return prefix + str.substr(0, 10);
}

static TestPbsDataConfig BuildTestDataConfig() {
  TestPbsDataConfig config;
  config.region = kRegion;
  config.network_name = GetRandomString("network");

  config.localstack_container_name = GetRandomString("localstack-container");
  config.localstack_port = kLocalstackPort;

  config.reporting_origin = kReportnigOrigin;

  return config;
}

static TestPbsConfig BuildTestPbsConfig(const string& pbs1_port,
                                        const string& pbs1_health_port,
                                        const string& pbs2_port,
                                        const string& pbs2_health_port) {
  TestPbsConfig config;
  config.pbs1_container_name = GetRandomString("pbs1-container-");
  config.pbs1_port = pbs1_port;
  config.pbs1_health_port = pbs1_health_port;
  config.pbs1_budget_key_table_name = GetRandomString("table");
  config.pbs1_partition_lock_table_name = GetRandomString("lock_table");
  config.pbs1_journal_bucket_name = GetRandomString("bucket");

  config.pbs2_container_name = GetRandomString("pbs2-container-");
  config.pbs2_port = pbs2_port;
  config.pbs2_health_port = pbs2_health_port;
  config.pbs2_budget_key_table_name = GetRandomString("table");
  config.pbs2_partition_lock_table_name = GetRandomString("lock_table");
  config.pbs2_journal_bucket_name = GetRandomString("bucket");

  return config;
}

static string CreateUrl(string host, string port) {
  return host + ":" + port;
}

static AsyncContext<ConsumeBudgetTransactionRequest,
                    ConsumeBudgetTransactionResponse>
CreateAsyncContext(ExecutionResult& execution_result, atomic<bool>& finished,
                   const ExecutionResult& expected_execution_result,
                   const Uuid& transaction_id) {
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.request =
      make_shared<ConsumeBudgetTransactionRequest>();
  consume_budget_transaction_context.request->budget_keys =
      make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  consume_budget_transaction_context.request->budget_keys->push_back(metadata);
  consume_budget_transaction_context.request->transaction_id = transaction_id;
  consume_budget_transaction_context.request->transaction_secret =
      make_shared<string>(kTransactionSecret);
  consume_budget_transaction_context
      .callback = [expected_execution_result, &execution_result,
                   &finished](AsyncContext<ConsumeBudgetTransactionRequest,
                                           ConsumeBudgetTransactionResponse>&
                                  consume_budget_transaction_context) {
    execution_result = consume_budget_transaction_context.result;
    if (!execution_result.Successful()) {
      std::cout << "Consume transaction error is "
                << GetErrorMessage(
                       consume_budget_transaction_context.result.status_code)
                << std::endl;
    }
    finished = true;
    EXPECT_EQ(execution_result, expected_execution_result);
  };

  return consume_budget_transaction_context;
}

class PBSIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
    config_ = BuildTestDataConfig();
    server_starter_ = make_unique<TestPbsServerStarter>(config_);
    auto result = server_starter_->Setup();
    if (result != 0) {
      throw runtime_error("Failed to setup server!");
    }
  }

  static void TearDownTestSuite() {
    server_starter_->Teardown();
    SDKOptions options;
    ShutdownAPI(options);
  }

  void SetUp() override {
    async_executor_ = make_shared<AsyncExecutor>(5, 10000,
                                                 /*drop_tasks_on_stop*/ true);
    http_client_ = make_shared<HttpClient>(
        async_executor_, core::common::RetryStrategyType::Linear, 1000, 50);

    EXPECT_EQ(async_executor_->Init(), SuccessExecutionResult());
    EXPECT_EQ(http_client_->Init(), SuccessExecutionResult());

    EXPECT_EQ(async_executor_->Run(), SuccessExecutionResult());
    EXPECT_EQ(http_client_->Run(), SuccessExecutionResult());

    dummy_auth_token_provider_ = make_shared<DummyTokenProviderCache>();
  }

  void StopPbsClientAndServer() {
    EXPECT_EQ(async_executor_->Stop(), SuccessExecutionResult());
    EXPECT_EQ(http_client_->Stop(), SuccessExecutionResult());
    EXPECT_EQ(pbs_client_->Stop(), SuccessExecutionResult());
    server_starter_->StopTwoPbsServers(pbs_config_);
  }

  void CreateAndRunPbsClient(const string& pbs_port,
                             const string& pbs_health_port) {
    pbs_client_ = make_shared<PrivacyBudgetServiceClient>(
        config_.reporting_origin, CreateUrl(kLocalHost, pbs_port), http_client_,
        dummy_auth_token_provider_);
    EXPECT_EQ(pbs_client_->Init(), SuccessExecutionResult());
    EXPECT_EQ(pbs_client_->Run(), SuccessExecutionResult());
  }

  void StopPbsTransactionalClientAndServer() {
    EXPECT_EQ(async_executor_->Stop(), SuccessExecutionResult());
    EXPECT_EQ(http_client_->Stop(), SuccessExecutionResult());
    EXPECT_EQ(pbs_transactional_client_->Stop(), SuccessExecutionResult());
    server_starter_->StopTwoPbsServers(pbs_config_);
  }

  void CreateAndRunPbsTransactionalClient() {
    pbs_transactional_client_ =
        make_shared<PrivacyBudgetServiceTransactionalClient>(
            config_.reporting_origin,
            CreateUrl(kLocalHost, pbs_config_.pbs1_port),
            CreateUrl(kLocalHost, pbs_config_.pbs2_port), http_client_,
            async_executor_, dummy_auth_token_provider_,
            dummy_auth_token_provider_);
    EXPECT_EQ(pbs_transactional_client_->Init(), SuccessExecutionResult());
    EXPECT_EQ(pbs_transactional_client_->Run(), SuccessExecutionResult());
  }

  Timestamp GetLastExecutionTime(const Uuid& transaction_id) {
    atomic<bool> finished = false;
    Timestamp timestamp = 0;
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
        get_transaction_status_context;
    get_transaction_status_context.request =
        make_shared<GetTransactionStatusRequest>();
    get_transaction_status_context.request->transaction_id = transaction_id;
    get_transaction_status_context.request->transaction_secret =
        make_shared<string>(kTransactionSecret);
    get_transaction_status_context.callback =
        [&](AsyncContext<GetTransactionStatusRequest,
                         GetTransactionStatusResponse>&
                get_transaction_context) {
          if (get_transaction_context.result.Successful()) {
            timestamp =
                get_transaction_context.response->last_execution_timestamp;
          } else {
            std::cout << "Get_transaction_context error is "
                      << GetErrorMessage(
                             get_transaction_context.result.status_code)
                      << std::endl;
          }
          finished = true;
          EXPECT_EQ(get_transaction_context.result, SuccessExecutionResult());
        };
    pbs_client_->GetTransactionStatus(get_transaction_status_context);
    WaitUntil([&]() { return finished.load(); }, std::chrono::seconds(30));
    if (timestamp == 0) {
      StopPbsClientAndServer();
    }
    return timestamp;
  }

  ExecutionResult ConsumeBudget(
      const ExecutionResult& expected_execution_result,
      const Uuid& transaction_id) {
    auto execution_result = SuccessExecutionResult();
    atomic<bool> finished = false;
    auto consume_budget_transaction_context = CreateAsyncContext(
        execution_result, finished, expected_execution_result, transaction_id);
    EXPECT_EQ(pbs_transactional_client_->ConsumeBudget(
                  consume_budget_transaction_context),
              SuccessExecutionResult());
    WaitUntil([&]() { return finished.load(); }, std::chrono::seconds(300));
    if (execution_result != expected_execution_result) {
      StopPbsTransactionalClientAndServer();
    }
    return execution_result;
  }

  ExecutionResult InitTransaction(const Uuid& transaction_id) {
    auto execution_result = SuccessExecutionResult();
    atomic<bool> finished = false;
    auto consume_budget_transaction_context = CreateAsyncContext(
        execution_result, finished, SuccessExecutionResult(), transaction_id);
    EXPECT_EQ(pbs_client_->InitiateConsumeBudgetTransaction(
                  consume_budget_transaction_context),
              SuccessExecutionResult());
    WaitUntil([&]() { return finished.load(); }, std::chrono::seconds(100));
    if (!execution_result.Successful()) {
      StopPbsClientAndServer();
    }
    return execution_result;
  }

  ExecutionResult ExecuteUntilTransactionPhase(TransactionExecutionPhase phase,
                                               const Uuid& transaction_id) {
    auto execution_result = InitTransaction(transaction_id);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    for (auto it = kTransactionPhases.begin(); it != kTransactionPhases.end();
         ++it) {
      if (*it == phase) break;
      auto execution_result = ExecuteTransactionPhase(*it, transaction_id);
      if (!execution_result.Successful()) {
        return execution_result;
      }
    }
    return SuccessExecutionResult();
  }

  ExecutionResult ExecuteTransactionPhase(TransactionExecutionPhase phase,
                                          const Uuid& transaction_id) {
    auto execution_result = SuccessExecutionResult();
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
        transaction_phase_context;
    transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
    transaction_phase_context.request->transaction_id = transaction_id;
    transaction_phase_context.request->transaction_secret =
        make_shared<string>(kTransactionSecret);
    transaction_phase_context.request->transaction_execution_phase = phase;
    auto timestamp = GetLastExecutionTime(transaction_id);
    if (timestamp == 0) return FailureExecutionResult(SC_UNKNOWN);
    transaction_phase_context.request->last_execution_timestamp = timestamp;
    atomic<bool> finished = false;
    transaction_phase_context.callback =
        [&finished, &execution_result](
            AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                context) {
          execution_result = context.result;
          if (!context.result.Successful()) {
            std::cout << "ExecuteTransactionPhase error is "
                      << GetErrorMessage(context.result.status_code)
                      << std::endl;
          }
          finished = true;
          EXPECT_EQ(context.result, SuccessExecutionResult());
        };

    EXPECT_EQ(pbs_client_->ExecuteTransactionPhase(transaction_phase_context),
              SuccessExecutionResult());
    WaitUntil([&]() { return finished.load(); }, std::chrono::seconds(30));
    if (!execution_result.Successful()) {
      StopPbsClientAndServer();
    }
    return execution_result;
  }

  void RestartPbsServers() {
    server_starter_->StopTwoPbsServers(pbs_config_);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto result =
        server_starter_->RunTwoPbsServers(pbs_config_, /*setup_data*/ false);
    if (result != 0) {
      throw runtime_error("Failed to run PBS server!");
    }
  }

  void RestartPbsServer1() {
    server_starter_->StopPbsServer1(pbs_config_);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto result = server_starter_->RunPbsServer1(pbs_config_);
    if (result != 0) {
      throw runtime_error("Failed to run PBS server!");
    }
  }

  shared_ptr<HttpClientInterface> http_client_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<PrivacyBudgetServiceClient> pbs_client_;
  shared_ptr<PrivacyBudgetServiceTransactionalClient> pbs_transactional_client_;
  shared_ptr<TokenProviderCacheInterface> dummy_auth_token_provider_;
  TestPbsConfig pbs_config_;
  static TestPbsDataConfig config_;
  static unique_ptr<TestPbsServerStarter> server_starter_;
};

TestPbsDataConfig PBSIntegrationTest::config_ = TestPbsDataConfig();
unique_ptr<TestPbsServerStarter> PBSIntegrationTest::server_starter_ = nullptr;

TEST_F(PBSIntegrationTest, TwoServersConsumeBudget) {
  string pbs1_port = "9948";
  string pbs1_health_port = "10000";
  string pbs2_port = "9949";
  string pbs2_health_port = "10001";
  pbs_config_ = BuildTestPbsConfig(pbs1_port, pbs1_health_port, pbs2_port,
                                   pbs2_health_port);
  auto result = server_starter_->RunTwoPbsServers(pbs_config_,
                                                  /*setup_data*/ true);
  if (result != 0) {
    throw runtime_error("Failed to run PBS servers!");
  }

  CreateAndRunPbsTransactionalClient();

  auto transaction_id = Uuid::GenerateUuid();
  auto execution_result =
      ConsumeBudget(SuccessExecutionResult(), transaction_id);
  if (!execution_result.Successful()) return;

  StopPbsTransactionalClientAndServer();
}

TEST_F(PBSIntegrationTest, OnlyOneTransactionSucceeded) {
  string pbs1_port = "9958";
  string pbs1_health_port = "10010";
  string pbs2_port = "9959";
  string pbs2_health_port = "10011";
  pbs_config_ = BuildTestPbsConfig(pbs1_port, pbs1_health_port, pbs2_port,
                                   pbs2_health_port);
  auto result =
      server_starter_->RunTwoPbsServers(pbs_config_, /*setup_data*/ true);
  if (result != 0) {
    throw runtime_error("Failed to run PBS servers!");
  }

  CreateAndRunPbsTransactionalClient();

  auto transaction_id = Uuid::GenerateUuid();
  auto execution_result =
      ConsumeBudget(SuccessExecutionResult(), transaction_id);
  if (!execution_result.Successful()) return;

  transaction_id = Uuid::GenerateUuid();
  auto expected_execution_result =
      FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT);
  execution_result = ConsumeBudget(expected_execution_result, transaction_id);
  if (execution_result != expected_execution_result) return;

  StopPbsTransactionalClientAndServer();
}

TEST_F(PBSIntegrationTest, BothServerAreDown) {
  string pbs1_port = "9968";
  string pbs1_health_port = "10020";
  string pbs2_port = "9969";
  string pbs2_health_port = "10021";
  pbs_config_ = BuildTestPbsConfig(pbs1_port, pbs1_health_port, pbs2_port,
                                   pbs2_health_port);
  auto result =
      server_starter_->RunTwoPbsServers(pbs_config_, /*setup_data*/ true);
  if (result != 0) {
    throw runtime_error("Failed to run PBS server!");
  }

  CreateAndRunPbsClient(pbs1_port, pbs1_health_port);

  auto transaction_id = Uuid::GenerateUuid();
  auto execution_result = InitTransaction(transaction_id);
  if (!execution_result.Successful()) return;

  RestartPbsServers();

  execution_result = ExecuteTransactionPhase(TransactionExecutionPhase::Prepare,
                                             transaction_id);
  if (!execution_result.Successful()) return;

  RestartPbsServers();

  execution_result = ExecuteTransactionPhase(TransactionExecutionPhase::Commit,
                                             transaction_id);
  if (!execution_result.Successful()) return;

  RestartPbsServers();

  execution_result = ExecuteTransactionPhase(TransactionExecutionPhase::Notify,
                                             transaction_id);
  if (!execution_result.Successful()) return;

  RestartPbsServers();

  execution_result =
      ExecuteTransactionPhase(TransactionExecutionPhase::End, transaction_id);
  if (!execution_result.Successful()) return;

  StopPbsClientAndServer();
}

TEST_F(PBSIntegrationTest, OneServerIsDown) {
  string pbs1_port = "9978";
  string pbs1_health_port = "10030";
  string pbs2_port = "9979";
  string pbs2_health_port = "10031";
  pbs_config_ = BuildTestPbsConfig(pbs1_port, pbs1_health_port, pbs2_port,
                                   pbs2_health_port);
  auto result =
      server_starter_->RunTwoPbsServers(pbs_config_, /*setup_data*/ true);
  if (result != 0) {
    throw runtime_error("Failed to run PBS server!");
  }

  CreateAndRunPbsClient(pbs1_port, pbs1_health_port);

  auto transaction_id = Uuid::GenerateUuid();
  auto execution_result = InitTransaction(transaction_id);
  if (!execution_result.Successful()) {
    return;
  }

  RestartPbsServer1();

  execution_result = ExecuteTransactionPhase(TransactionExecutionPhase::Prepare,
                                             transaction_id);
  if (!execution_result.Successful()) return;

  RestartPbsServer1();

  execution_result = ExecuteTransactionPhase(TransactionExecutionPhase::Commit,
                                             transaction_id);
  if (!execution_result.Successful()) return;

  RestartPbsServer1();

  execution_result = ExecuteTransactionPhase(TransactionExecutionPhase::Notify,
                                             transaction_id);
  if (!execution_result.Successful()) return;

  RestartPbsServer1();

  execution_result =
      ExecuteTransactionPhase(TransactionExecutionPhase::End, transaction_id);
  if (!execution_result.Successful()) return;

  StopPbsClientAndServer();
}

TEST_F(PBSIntegrationTest, AbortTransaction) {
  string pbs1_port = "9988";
  string pbs1_health_port = "10040";
  string pbs2_port = "9989";
  string pbs2_health_port = "10041";
  pbs_config_ = BuildTestPbsConfig(pbs1_port, pbs1_health_port, pbs2_port,
                                   pbs2_health_port);
  auto result =
      server_starter_->RunTwoPbsServers(pbs_config_, /*setup_data*/ true);
  if (result != 0) {
    throw runtime_error("Failed to run PBS server!");
  }

  CreateAndRunPbsClient(pbs1_port, pbs1_health_port);

  auto transaction_id = Uuid::GenerateUuid();
  auto execution_result = ExecuteUntilTransactionPhase(
      TransactionExecutionPhase::Prepare, transaction_id);
  if (!execution_result.Successful()) return;

  execution_result =
      ExecuteTransactionPhase(TransactionExecutionPhase::Abort, transaction_id);
  if (!execution_result.Successful()) return;

  transaction_id = Uuid::GenerateUuid();
  execution_result = ExecuteUntilTransactionPhase(
      TransactionExecutionPhase::Commit, transaction_id);
  if (!execution_result.Successful()) return;

  execution_result =
      ExecuteTransactionPhase(TransactionExecutionPhase::Abort, transaction_id);
  if (!execution_result.Successful()) return;

  transaction_id = Uuid::GenerateUuid();
  execution_result = ExecuteUntilTransactionPhase(
      TransactionExecutionPhase::Notify, transaction_id);
  if (!execution_result.Successful()) return;

  execution_result =
      ExecuteTransactionPhase(TransactionExecutionPhase::Abort, transaction_id);
  if (!execution_result.Successful()) return;

  transaction_id = Uuid::GenerateUuid();
  execution_result = ExecuteUntilTransactionPhase(
      TransactionExecutionPhase::End, transaction_id);
  if (!execution_result.Successful()) return;

  execution_result =
      ExecuteTransactionPhase(TransactionExecutionPhase::End, transaction_id);
  if (!execution_result.Successful()) return;

  StopPbsClientAndServer();
}
}  // namespace google::scp::pbs::test::e2e
