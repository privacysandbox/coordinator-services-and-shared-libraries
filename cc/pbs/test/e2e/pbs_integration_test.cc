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
#include "core/common/operation_dispatcher/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/error_codes.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/errors.h"
#include "core/interface/token_provider_cache_interface.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "pbs/pbs_client/src/transactional/pbs_transactional_client.h"
#include "pbs/test/e2e/test_pbs_server_starter/test_pbs_server_starter.h"
#include "public/core/test/interface/execution_result_matchers.h"

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
using google::scp::core::HttpClientOptions;
using google::scp::core::LoggerInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::Timestamp;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::core::errors::GetErrorHttpStatusCode;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::TestTimeoutException;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::pbs::PrivacyBudgetServiceTransactionalClient;
using google::scp::pbs::test::e2e::AuthResult;
using google::scp::pbs::test::e2e::TestPbsConfig;
using google::scp::pbs::test::e2e::TestPbsDataConfig;
using google::scp::pbs::test::e2e::TestPbsServerStarter;
using std::atomic;
using std::cout;
using std::endl;
using std::make_shared;
using std::make_unique;
using std::map;
using std::mt19937;
using std::pair;
using std::random_device;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::seconds;
using std::this_thread::sleep_for;

static constexpr char kRegion[] = "us-east-1";
static constexpr char kLocalHost[] = "http://127.0.0.1";
static constexpr char kReportingOrigin[] = "test.com";
// TODO(b/241857324): pick available ports randomly.
static constexpr char kLocalstackPort[] = "4566";
static constexpr char kTransactionSecret[] = "transaction_secret";

static constexpr size_t kAsyncExecutorQueueSizeValue = 5;
static constexpr size_t kAsyncExecutorQueueCapValue = 10000;

// NOTE: Transaction Timeout should be atleast the
// kDefaultPBSRequestWaitTimeInSeconds in these tests.
static constexpr TimeDuration kTransactionTimeoutInSeconds = 120;
static constexpr TimeDuration kHTTPClientBackoffDurationInMs = 2000;
static constexpr size_t kHTTPClientMaxRetries = 6;
static constexpr TimeDuration kHttp2ReadTimeoutInSeconds = 10;
// NOTE: kDefaultPBSRequestWaitTimeInSeconds > sum(1, 2, 3 ...,
// kHTTPClientMaxRetries)
static constexpr seconds kDefaultPBSRequestWaitTimeInSeconds = seconds(60);

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

  config.reporting_origin = kReportingOrigin;

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
CreateConsumeBudgetRequest(atomic<bool>& finished,
                           const ExecutionResult expected_execution_result,
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
  consume_budget_transaction_context.callback =
      [expected_execution_result,
       &finished](AsyncContext<ConsumeBudgetTransactionRequest,
                               ConsumeBudgetTransactionResponse>&
                      consume_budget_transaction_context) {
        EXPECT_THAT(consume_budget_transaction_context.result,
                    ResultIs(expected_execution_result));
        finished = true;
      };
  return consume_budget_transaction_context;
}

static void EnsureBothPBSServersAreTakingClientRequests(
    shared_ptr<PrivacyBudgetServiceTransactionalClient>
        pbs_transactional_client,
    seconds wait_time_in_seconds = kDefaultPBSRequestWaitTimeInSeconds) {
  cout << "Making sure PBS servers are taking client requests..." << endl;

  atomic<bool> should_retry_request(true);
  atomic<int> request_flow_exited_count(0);

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      request_context1;
  request_context1.request = make_shared<GetTransactionStatusRequest>();
  request_context1.request->transaction_id = kZeroUuid;
  request_context1.request->transaction_secret =
      make_shared<string>(kTransactionSecret);
  request_context1.callback = [&should_retry_request,
                               &request_flow_exited_count,
                               pbs_transactional_client](
                                  AsyncContext<GetTransactionStatusRequest,
                                               GetTransactionStatusResponse>
                                      response_context) {
    auto http_code =
        GetErrorHttpStatusCode(response_context.result.status_code);
    if (http_code < core::errors::HttpStatusCode::INTERNAL_SERVER_ERROR) {
      cout << "PBS-1 endpoint accepts client requests. Status code: "
           << static_cast<int>(http_code) << endl;
      request_flow_exited_count++;
    } else if (should_retry_request) {
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
          new_request_context;
      new_request_context.request = response_context.request;
      new_request_context.callback = response_context.callback;
      cout << "PBS-1 endpoint is not up yet... Retrying.. Status Code: "
           << static_cast<int>(http_code) << endl;
      EXPECT_SUCCESS(pbs_transactional_client->GetTransactionStatusOnPBS1(
          new_request_context));
    } else {
      request_flow_exited_count++;
    }
  };
  EXPECT_SUCCESS(
      pbs_transactional_client->GetTransactionStatusOnPBS1(request_context1));

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      request_context2;
  request_context2.request = make_shared<GetTransactionStatusRequest>();
  request_context2.request->transaction_id = kZeroUuid;
  request_context2.request->transaction_secret =
      make_shared<string>(kTransactionSecret);
  request_context2.callback = [&should_retry_request,
                               &request_flow_exited_count,
                               pbs_transactional_client](
                                  AsyncContext<GetTransactionStatusRequest,
                                               GetTransactionStatusResponse>
                                      response_context) {
    auto http_code =
        GetErrorHttpStatusCode(response_context.result.status_code);
    if (http_code < core::errors::HttpStatusCode::INTERNAL_SERVER_ERROR) {
      cout << "PBS-2 endpoint accepts client requests. Status Code: "
           << static_cast<int>(http_code) << endl;
      request_flow_exited_count++;
    } else if (should_retry_request) {
      cout << "PBS-2 endpoint is not up yet... Retrying.. Status Code: "
           << static_cast<int>(http_code) << endl;
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
          new_request_context;
      new_request_context.request = response_context.request;
      new_request_context.callback = response_context.callback;
      EXPECT_SUCCESS(pbs_transactional_client->GetTransactionStatusOnPBS2(
          new_request_context));
    } else {
      request_flow_exited_count++;
    }
  };
  EXPECT_SUCCESS(
      pbs_transactional_client->GetTransactionStatusOnPBS2(request_context2));

  cout << "Waiting for both the PBS servers to take client requests..." << endl;
  try {
    WaitUntil([&]() { return request_flow_exited_count == 2; },
              wait_time_in_seconds);
  } catch (TestTimeoutException exception) {
    // Flip the retry to false so that the request flows would exit.
    should_retry_request = false;
    // Wait until requests are done.
    while (request_flow_exited_count != 2) {}
    throw exception;
  }
}

static shared_ptr<AsyncExecutor> CreateAsyncExecutor() {
  return make_shared<AsyncExecutor>(kAsyncExecutorQueueSizeValue,
                                    kAsyncExecutorQueueCapValue,
                                    /*drop_tasks_on_stop*/ true);
}

static HttpClientOptions CreateHttpClientOptions() {
  return HttpClientOptions(RetryStrategyOptions(RetryStrategyType::Linear,
                                                kHTTPClientBackoffDurationInMs,
                                                kHTTPClientMaxRetries),
                           core::kDefaultMaxConnectionsPerHost,
                           kHttp2ReadTimeoutInSeconds);
}

class PBSIntegrationTestForTwoServers : public ::testing::Test {
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
    TestLoggingUtils::EnableLogOutputToConsole();
  }

  static void TearDownTestSuite() {
    server_starter_->Teardown();
    SDKOptions options;
    ShutdownAPI(options);
  }

  void SetUp() override {
    async_executor_ = CreateAsyncExecutor();

    auto options = CreateHttpClientOptions();
    http_client_ = make_shared<HttpClient>(async_executor_, options);

    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(http_client_->Init());

    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(http_client_->Run());

    dummy_auth_token_provider_ = make_shared<DummyTokenProviderCache>();

    // Starting port picked arbitrarily.
    static atomic<int> port_counter = 9948;

    string pbs1_port = to_string(port_counter++);
    string pbs1_health_port = to_string(port_counter++);
    string pbs2_port = to_string(port_counter++);
    string pbs2_health_port = to_string(port_counter++);

    pbs_config_ = BuildTestPbsConfig(pbs1_port, pbs1_health_port, pbs2_port,
                                     pbs2_health_port);

    // Add any config overrides for all the test cases here.
    map<string, string> env_overrides;
    env_overrides[core::kTransactionTimeoutInSecondsConfigName] =
        kTransactionTimeoutInSeconds;
    env_overrides[core::kPBSAuthorizationEnableSiteBasedAuthorization] = "true";
    env_overrides[kServiceMetricsBatchPush] = "true";
    env_overrides[kServiceMetricsNamespace] = "pbs";

    auto result =
        server_starter_->RunTwoPbsServers(pbs_config_,
                                          /*setup_data*/ true, env_overrides);
    if (result != 0) {
      throw runtime_error("Failed to run PBS servers!");
    }

    two_pbs_transactional_client_ =
        make_shared<PrivacyBudgetServiceTransactionalClient>(
            config_.reporting_origin,
            CreateUrl(kLocalHost, pbs_config_.pbs1_port),
            CreateUrl(kLocalHost, pbs_config_.pbs2_port), http_client_,
            async_executor_, dummy_auth_token_provider_,
            dummy_auth_token_provider_);
    EXPECT_SUCCESS(two_pbs_transactional_client_->Init());
    EXPECT_SUCCESS(two_pbs_transactional_client_->Run());

    pbs1_client_ = make_shared<PrivacyBudgetServiceClient>(
        config_.reporting_origin, CreateUrl(kLocalHost, pbs_config_.pbs1_port),
        http_client_, dummy_auth_token_provider_);
    EXPECT_SUCCESS(pbs1_client_->Init());
    EXPECT_SUCCESS(pbs1_client_->Run());

    pbs2_client_ = make_shared<PrivacyBudgetServiceClient>(
        config_.reporting_origin, CreateUrl(kLocalHost, pbs_config_.pbs2_port),
        http_client_, dummy_auth_token_provider_);
    EXPECT_SUCCESS(pbs2_client_->Init());
    EXPECT_SUCCESS(pbs2_client_->Run());

    EnsureBothPBSServersAreTakingClientRequests(two_pbs_transactional_client_);
  }

  void TearDown() override {
    EXPECT_SUCCESS(pbs1_client_->Stop());
    EXPECT_SUCCESS(pbs2_client_->Stop());
    EXPECT_SUCCESS(two_pbs_transactional_client_->Stop());
    EXPECT_SUCCESS(http_client_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
    server_starter_->StopTwoPbsServers(pbs_config_);
    // Add some sleep before we start another test which starts PBS servers
    // again.
    sleep_for(seconds(5));
  }

  pair<Timestamp, TransactionExecutionPhase>
  GetLastExecutionTimeAndTransactionPhase(
      shared_ptr<PrivacyBudgetServiceClient> pbs_client,
      const Uuid& transaction_id) {
    atomic<bool> finished = false;
    Timestamp timestamp = 0;
    TransactionExecutionPhase transaction_execution_phase =
        TransactionExecutionPhase::Unknown;
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
            transaction_execution_phase =
                get_transaction_context.response->transaction_execution_phase;
          } else {
            cout << "Get_transaction_context error is "
                 << GetErrorMessage(get_transaction_context.result.status_code)
                 << endl;
          }
          EXPECT_SUCCESS(get_transaction_context.result);
          finished = true;
        };
    pbs_client->GetTransactionStatus(get_transaction_status_context);
    WaitUntil([&]() { return finished.load(); },
              kDefaultPBSRequestWaitTimeInSeconds);
    return {timestamp, transaction_execution_phase};
  }

  void ExecuteConsumeBudgetOnTwoPBSServers(
      shared_ptr<PrivacyBudgetServiceTransactionalClient>
          pbs_transactional_client,
      const ExecutionResult& expected_execution_result,
      const Uuid& transaction_id) {
    atomic<bool> request_finished = false;
    auto request_context = CreateConsumeBudgetRequest(
        request_finished, expected_execution_result, transaction_id);
    EXPECT_SUCCESS(
        two_pbs_transactional_client_->ConsumeBudget(request_context));
    WaitUntil([&]() { return request_finished.load(); }, seconds(300));
  }

  void ExecuteBeginTransactionOnPBSServer(
      shared_ptr<PrivacyBudgetServiceClient> pbs_client,
      const Uuid& transaction_id,
      ExecutionResult expected_execution_result = SuccessExecutionResult()) {
    atomic<bool> request_finished = false;
    auto request_context = CreateConsumeBudgetRequest(
        request_finished, expected_execution_result, transaction_id);
    auto txn_id_string = ToString(transaction_id);
    cout << "Initiating begin transaction for id " << txn_id_string << endl;
    EXPECT_SUCCESS(
        pbs_client->InitiateConsumeBudgetTransaction(request_context));
    WaitUntil([&]() { return request_finished.load(); }, seconds(100));
  }

  void ExecuteUntilAndIncludingTransactionPhase(
      shared_ptr<PrivacyBudgetServiceClient> pbs_client,
      const vector<TransactionExecutionPhase>& phases_to_execute,
      TransactionExecutionPhase phase, const Uuid& transaction_id) {
    bool is_final_phase_to_execute = false;
    for (auto it = phases_to_execute.begin(); it != phases_to_execute.end();
         ++it) {
      if (*it == phase) {
        is_final_phase_to_execute = true;
      }
      if (*it == TransactionExecutionPhase::Begin) {
        ExecuteBeginTransactionOnPBSServer(pbs_client, transaction_id);
      } else {
        ExecuteTransactionPhaseOnPBSServer(pbs_client, *it, transaction_id);
      }
      if (is_final_phase_to_execute) {
        break;
      }
    }
  }

  void ExecuteTransactionPhaseOnPBSServer(
      shared_ptr<PrivacyBudgetServiceClient> pbs_client,
      TransactionExecutionPhase phase, const Uuid& transaction_id,
      ExecutionResult expected_execution_result = SuccessExecutionResult()) {
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
        transaction_phase_context;
    transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
    transaction_phase_context.request->transaction_id = transaction_id;
    transaction_phase_context.request->transaction_secret =
        make_shared<string>(kTransactionSecret);
    transaction_phase_context.request->transaction_execution_phase = phase;
    auto timestamp_phase_pair =
        GetLastExecutionTimeAndTransactionPhase(pbs_client, transaction_id);
    auto txn_id_string = ToString(transaction_id);
    cout << "Executing phase: " << static_cast<int>(phase)
         << " transaction for id " << txn_id_string << endl;
    cout << "Obtained last execution time: " << timestamp_phase_pair.first
         << endl;
    cout << "Obtained TransactionExecutionPhase: "
         << static_cast<int>(timestamp_phase_pair.second) << endl;
    transaction_phase_context.request->last_execution_timestamp =
        timestamp_phase_pair.first;
    atomic<bool> finished = false;
    transaction_phase_context.callback =
        [&finished, expected_execution_result](
            AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                context) {
          if (!context.result.Successful()) {
            cout << "ExecuteTransactionPhaseOnPBSServer error is "
                 << GetErrorMessage(context.result.status_code) << endl;
          }
          EXPECT_THAT(context.result, ResultIs(expected_execution_result));
          finished = true;
        };
    EXPECT_SUCCESS(
        pbs_client->ExecuteTransactionPhase(transaction_phase_context));
    WaitUntil([&]() { return finished.load(); },
              kDefaultPBSRequestWaitTimeInSeconds);
  }

  void RestartBothPBSServers(map<string, string> env_overrides = {}) {
    server_starter_->StopTwoPbsServers(pbs_config_);
    sleep_for(seconds(5));
    auto result = server_starter_->RunTwoPbsServers(
        pbs_config_, /*setup_data*/ false, env_overrides);
    if (result != 0) {
      throw runtime_error("Failed to run PBS server!");
    }
  }

  void RestartPBS1Server(map<string, string> env_overrides = {}) {
    server_starter_->StopPbsServer1(pbs_config_);
    sleep_for(seconds(5));
    auto result = server_starter_->RunPbsServer1(pbs_config_, env_overrides);
    if (result != 0) {
      throw runtime_error("Failed to run PBS server!");
    }
  }

  void RestartPBS2Server(map<string, string> env_overrides = {}) {
    server_starter_->StopPbsServer2(pbs_config_);
    sleep_for(seconds(5));
    auto result = server_starter_->RunPbsServer2(pbs_config_, env_overrides);
    if (result != 0) {
      throw runtime_error("Failed to run PBS server!");
    }
  }

  shared_ptr<HttpClientInterface> http_client_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<PrivacyBudgetServiceClient> pbs1_client_;
  shared_ptr<PrivacyBudgetServiceClient> pbs2_client_;
  shared_ptr<PrivacyBudgetServiceTransactionalClient>
      two_pbs_transactional_client_;
  shared_ptr<TokenProviderCacheInterface> dummy_auth_token_provider_;
  TestPbsConfig pbs_config_;
  static TestPbsDataConfig config_;
  static unique_ptr<TestPbsServerStarter> server_starter_;
  vector<TransactionExecutionPhase> committed_transaction_phases_ = {
      TransactionExecutionPhase::Begin, TransactionExecutionPhase::Prepare,
      TransactionExecutionPhase::Commit, TransactionExecutionPhase::Notify,
      TransactionExecutionPhase::End};
};

TestPbsDataConfig PBSIntegrationTestForTwoServers::config_ =
    TestPbsDataConfig();
unique_ptr<TestPbsServerStarter>
    PBSIntegrationTestForTwoServers::server_starter_ = nullptr;

TEST(PBSIntegrationTestHelperTester,
     EnsureBothPBSServersAreTakingClientRequests) {
  shared_ptr<AsyncExecutorInterface> async_executor = CreateAsyncExecutor();
  HttpClientOptions http_options(
      RetryStrategyOptions(RetryStrategyType::Linear,
                           1000 /* HTTPClientBackoffDurationInMs */,
                           3 /* HTTPClientMaxRetries */),
      core::kDefaultMaxConnectionsPerHost, kHttp2ReadTimeoutInSeconds);
  auto http_client = make_shared<HttpClient>(async_executor, http_options);

  auto auth_provider = make_shared<DummyTokenProviderCache>();
  EXPECT_SUCCESS(async_executor->Init());
  EXPECT_SUCCESS(http_client->Init());
  EXPECT_SUCCESS(async_executor->Run());
  EXPECT_SUCCESS(http_client->Run());

  // One PBS server is up.
  auto pbs_client1 = make_shared<PrivacyBudgetServiceTransactionalClient>(
      "ReportingOrigin", "https://google.com", "https://hostisunavailable:1",
      http_client, async_executor, auth_provider, auth_provider);
  EXPECT_SUCCESS(pbs_client1->Init());
  EXPECT_SUCCESS(pbs_client1->Run());
  EXPECT_THROW(
      EnsureBothPBSServersAreTakingClientRequests(pbs_client1, seconds(3)),
      TestTimeoutException);
  EXPECT_SUCCESS(pbs_client1->Stop());

  // Two PBS servers are up.
  auto pbs_client2 = make_shared<PrivacyBudgetServiceTransactionalClient>(
      "ReportingOrigin", "https://google.com", "https://facebook.com",
      http_client, async_executor, auth_provider, auth_provider);
  EXPECT_SUCCESS(pbs_client2->Init());
  EXPECT_SUCCESS(pbs_client2->Run());
  EXPECT_NO_THROW(
      EnsureBothPBSServersAreTakingClientRequests(pbs_client2, seconds(3)));
  EXPECT_SUCCESS(pbs_client2->Stop());

  // Neither of PBS servers are up.
  auto pbs_client3 = make_shared<PrivacyBudgetServiceTransactionalClient>(
      "ReportingOrigin", "https://hostisunavailable:1234",
      "https://hostisunavailable:1", http_client, async_executor, auth_provider,
      auth_provider);
  EXPECT_SUCCESS(pbs_client3->Init());
  EXPECT_SUCCESS(pbs_client3->Run());
  EXPECT_THROW(
      EnsureBothPBSServersAreTakingClientRequests(pbs_client3, seconds(3)),
      TestTimeoutException);
  EXPECT_SUCCESS(pbs_client3->Stop());

  EXPECT_SUCCESS(http_client->Stop());
  EXPECT_SUCCESS(async_executor->Stop());
}

TEST_F(PBSIntegrationTestForTwoServers,
       BudgetConsumptionIsSuccessfulOnBothServers) {
  ExecuteConsumeBudgetOnTwoPBSServers(two_pbs_transactional_client_,
                                      SuccessExecutionResult(),
                                      Uuid::GenerateUuid());
}

TEST_F(PBSIntegrationTestForTwoServers, DoubleConsumptionIsDisallowed) {
  // Budget consumption is the same for both the calls.
  ExecuteConsumeBudgetOnTwoPBSServers(two_pbs_transactional_client_,
                                      SuccessExecutionResult(),
                                      Uuid::GenerateUuid());
  ExecuteConsumeBudgetOnTwoPBSServers(
      two_pbs_transactional_client_,
      FailureExecutionResult(
          core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT),
      Uuid::GenerateUuid());
}

TEST_F(PBSIntegrationTestForTwoServers,
       ServerRestartsDuringTransactionExecutionOnSinglePBS) {
  auto transaction_id = Uuid::GenerateUuid();
  ExecuteBeginTransactionOnPBSServer(pbs1_client_, transaction_id);

  RestartPBS1Server();

  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Prepare, transaction_id);

  RestartPBS1Server();

  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Commit, transaction_id);

  RestartPBS1Server();

  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Notify, transaction_id);

  RestartPBS1Server();

  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::End, transaction_id);
}

TEST_F(PBSIntegrationTestForTwoServers, TransactionCanBeAbortedAtAnyPhase) {
  auto transaction_id = Uuid::GenerateUuid();

  ExecuteUntilAndIncludingTransactionPhase(
      pbs1_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Begin, transaction_id);
  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Abort, transaction_id);

  transaction_id = Uuid::GenerateUuid();
  ExecuteUntilAndIncludingTransactionPhase(
      pbs1_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Prepare, transaction_id);
  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Abort, transaction_id);

  transaction_id = Uuid::GenerateUuid();
  ExecuteUntilAndIncludingTransactionPhase(
      pbs1_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Commit, transaction_id);
  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Abort, transaction_id);

  transaction_id = Uuid::GenerateUuid();
  ExecuteUntilAndIncludingTransactionPhase(
      pbs1_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Notify, transaction_id);

  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::End, transaction_id);
}

TEST_F(PBSIntegrationTestForTwoServers,
       PBSServerResolvesTransactionFromOtherPBSServer) {
  // Set a low timeout for Transaction Expiry to speed up the test run.
  map<string, string> env_overrides;
  env_overrides[core::kTransactionTimeoutInSecondsConfigName] = "5";

  // Restart is required for config to be propogated.
  RestartBothPBSServers(env_overrides);
  EnsureBothPBSServersAreTakingClientRequests(two_pbs_transactional_client_);

  // Execute until Phase 1 of 2PC protocol
  auto transaction_id = Uuid::GenerateUuid();
  ExecuteUntilAndIncludingTransactionPhase(
      pbs1_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Commit, transaction_id);
  ExecuteUntilAndIncludingTransactionPhase(
      pbs2_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Commit, transaction_id);

  // Ensure transaction executed until Commit
  auto timestamp_phase_pair1 =
      GetLastExecutionTimeAndTransactionPhase(pbs1_client_, transaction_id);
  EXPECT_EQ(timestamp_phase_pair1.second, TransactionExecutionPhase::Notify);
  auto timestamp_phase_pair2 =
      GetLastExecutionTimeAndTransactionPhase(pbs2_client_, transaction_id);
  EXPECT_EQ(timestamp_phase_pair2.second, TransactionExecutionPhase::Notify);

  // Execute Phase 2 of 2PC protocol but only on one server i.e. PBS1
  // and ensure that it executed until Notify.
  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Notify, transaction_id);
  timestamp_phase_pair1 =
      GetLastExecutionTimeAndTransactionPhase(pbs1_client_, transaction_id);
  EXPECT_EQ(timestamp_phase_pair1.second, TransactionExecutionPhase::End);

  // Ensure transaction still only executed until Commit phase on PBS2.
  timestamp_phase_pair2 =
      GetLastExecutionTimeAndTransactionPhase(pbs2_client_, transaction_id);
  EXPECT_EQ(timestamp_phase_pair2.second, TransactionExecutionPhase::Notify);

  // Wait for transaction timeout
  // TODO: Make Transaction Cache Entry lifetime configurable. This reduces
  // the sleep time here. b/277647896. Wait until transaction expires. Wait
  // time should be atleast Transaction Timeout (5 seconds) + 2 * Transaction
  // Cache Lifetime (60 seconds)
  sleep_for(seconds(66));

  // Expect that the transaction resolved on PBS2 by taking to PBS1
  timestamp_phase_pair2 =
      GetLastExecutionTimeAndTransactionPhase(pbs2_client_, transaction_id);
  EXPECT_EQ(timestamp_phase_pair2.second, TransactionExecutionPhase::End);

  // Ensure budget consumption is disallowed on same budget on both the PBS
  // servers.
  auto transaction_id2 = Uuid::GenerateUuid();
  ExecuteBeginTransactionOnPBSServer(pbs1_client_, transaction_id2);
  ExecuteBeginTransactionOnPBSServer(pbs2_client_, transaction_id2);
  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Prepare, transaction_id2,
      ExecutionResult(FailureExecutionResult(
          core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT)));
  ExecuteTransactionPhaseOnPBSServer(
      pbs2_client_, TransactionExecutionPhase::Prepare, transaction_id2,
      ExecutionResult(FailureExecutionResult(
          core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT)));
}

TEST_F(PBSIntegrationTestForTwoServers,
       BudgetIsLockedAndDisallowsOtherConcurrentBudgetConsumptionRequests) {
  // Transaction 1 executes the Phase 1 of 2PC protocol
  auto transaction_id1 = Uuid::GenerateUuid();
  ExecuteUntilAndIncludingTransactionPhase(
      pbs1_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Commit, transaction_id1);
  ExecuteUntilAndIncludingTransactionPhase(
      pbs2_client_, committed_transaction_phases_,
      TransactionExecutionPhase::Commit, transaction_id1);

  // Transaction 2 tries to Prepare on same budget key and fails because the
  // budget is locked by Transaction 1
  auto transaction_id2 = Uuid::GenerateUuid();
  ExecuteBeginTransactionOnPBSServer(pbs1_client_, transaction_id2);
  ExecuteBeginTransactionOnPBSServer(pbs2_client_, transaction_id2);

  // The following fails due as pre-condition failed as the transation fails
  // to acquire. The failure manifests as 503
  // SC_HTTP2_CLIENT_HTTP_STATUS_SERVICE_UNAVAILABLE and dispatcher retries
  // until exhaustion.
  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Prepare, transaction_id2,
      FailureExecutionResult(core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES));
  ExecuteTransactionPhaseOnPBSServer(
      pbs2_client_, TransactionExecutionPhase::Prepare, transaction_id2,
      FailureExecutionResult(core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES));

  // Transaction 1 decides execute Phase 2 of 2PC protocol
  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::Notify, transaction_id1);
  ExecuteTransactionPhaseOnPBSServer(
      pbs2_client_, TransactionExecutionPhase::Notify, transaction_id1);

  ExecuteTransactionPhaseOnPBSServer(
      pbs1_client_, TransactionExecutionPhase::End, transaction_id1);
  ExecuteTransactionPhaseOnPBSServer(
      pbs2_client_, TransactionExecutionPhase::End, transaction_id1);

  // Any other transaction should fail with budget consumption failure.
  ExecuteConsumeBudgetOnTwoPBSServers(
      two_pbs_transactional_client_,
      FailureExecutionResult(
          core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT),
      Uuid::GenerateUuid());
}
}  // namespace google::scp::pbs::test::e2e
