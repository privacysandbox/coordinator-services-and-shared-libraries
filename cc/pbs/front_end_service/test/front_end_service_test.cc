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

#include "pbs/front_end_service/src/front_end_service.h"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/common/uuid/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_server/mock/mock_http2_server.h"
#include "core/interface/async_context.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/http_server_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/front_end_service/mock/mock_front_end_service_with_overrides.h"
#include "pbs/front_end_service/src/error_codes.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/partition_request_router/mock/mock_transaction_request_router.h"
#include "pbs/transactions/mock/mock_consume_budget_command_factory.h"
#include "pbs/transactions/src/batch_consume_budget_command.h"
#include "pbs/transactions/src/consume_budget_command.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::HttpServerInterface;
using google::scp::core::JournalServiceInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::RemoteTransactionManagerInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TransactionCommandSerializerInterface;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionManagerInterface;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::http2_server::mock::MockHttp2Server;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::AggregateMetricInterface;
using google::scp::cpio::MockAggregateMetric;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::BatchConsumeBudgetCommand;
using google::scp::pbs::BudgetKeyProviderInterface;
using google::scp::pbs::ConsumeBudgetCommand;
using google::scp::pbs::front_end_service::mock::
    MockFrontEndServiceWithOverrides;
using google::scp::pbs::partition_request_router::mock::
    MockTransactionRequestRouter;
using google::scp::pbs::transactions::mock::MockConsumeBudgetCommandFactory;
using std::atomic;
using std::dynamic_pointer_cast;
using std::list;
using std::make_pair;
using std::make_shared;
using std::move;
using std::pair;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;
using std::chrono::hours;
using std::chrono::nanoseconds;
using ::testing::_;
using ::testing::An;
using ::testing::DoAll;
using ::testing::Return;

namespace google::scp::pbs::test {

std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
GetMockConsumeBudgetCommandFactory() {
  return std::make_unique<MockConsumeBudgetCommandFactory>();
}

std::unique_ptr<MockTransactionRequestRouter>
GetMockTransactionRequestRouter() {
  return std::make_unique<MockTransactionRequestRouter>();
}

class FrontEndServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    InitializeClassComponents();
    InitializeTransactionContext();
  }

  void InitializeClassComponents() {
    auto mock_metric_client = make_shared<MockMetricClient>();
    mock_config_provider_ = make_shared<MockConfigProvider>();
    shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
    shared_ptr<JournalServiceInterface> journal_service;
    async_executor_ = make_shared<MockAsyncExecutor>();
    shared_ptr<TransactionCommandSerializerInterface>
        transaction_command_serializer;

    shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
        make_shared<MockNoSQLDatabaseProvider>();
    std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
        consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();

    auto transaction_request_router = GetMockTransactionRequestRouter();
    mock_transaction_request_router_ = transaction_request_router.get();

    shared_ptr<HttpServerInterface> http2_server =
        make_shared<MockHttp2Server>();
    front_end_service_ = make_shared<MockFrontEndServiceWithOverrides>(
        http2_server, async_executor_, std::move(transaction_request_router),
        std::move(consume_budget_command_factory), mock_metric_client,
        mock_config_provider_);

    front_end_service_->InitMetricInstances();
  }

  void InitializeTransactionContext() {
    transaction_context_.request = make_shared<TransactionRequest>();
    transaction_context_.request->transaction_id = Uuid::GenerateUuid();
    transaction_context_.request->transaction_secret =
        make_shared<string>("secret");
    transaction_context_.request->transaction_origin =
        make_shared<string>("origin");
    transaction_context_.result = FailureExecutionResult(SC_UNKNOWN);
    transaction_context_.response = make_shared<TransactionResponse>();
    transaction_context_.response->transaction_id =
        transaction_context_.request->transaction_id;
  }

  class BatchConsumeBudgetCommandOverride : public BatchConsumeBudgetCommand {
   public:
    explicit BatchConsumeBudgetCommandOverride(
        core::common::Uuid transaction_id,
        std::shared_ptr<BudgetKeyName>& budget_key_name,
        std::vector<ConsumeBudgetCommandRequestInfo>&& budget_consumptions)
        : BatchConsumeBudgetCommand(transaction_id, budget_key_name,
                                    std::move(budget_consumptions),
                                    nullptr /* not needed for the test */,
                                    nullptr /* not needed for the test */) {}

    void SetFailedBudgetsWithInsufficientConsumption(
        const vector<ConsumeBudgetCommandRequestInfo>& failed_budgets) {
      failed_insufficient_budget_consumptions_ = failed_budgets;
    }
  };

  class ConsumeBudgetCommandOverride : public ConsumeBudgetCommand {
   public:
    explicit ConsumeBudgetCommandOverride(
        core::common::Uuid transaction_id,
        std::shared_ptr<BudgetKeyName>& budget_key_name,
        ConsumeBudgetCommandRequestInfo&& budget_consumption)
        : ConsumeBudgetCommand(transaction_id, budget_key_name,
                               std::move(budget_consumption),
                               nullptr /* not needed for the test */,
                               nullptr /* not needed for the test */) {}

    void SetBudgetFailedDueToInsufficientConsumption() {
      failed_with_insufficient_budget_consumption_ = true;
    }
  };

  shared_ptr<BatchConsumeBudgetCommandOverride>
  GetBatchConsumeBudgetCommandOverride(
      core::common::Uuid& transaction_id, shared_ptr<string> budget_key,
      const vector<ConsumeBudgetCommandRequestInfo>& budget_consumptions) {
    auto budget_consumptions_copy = budget_consumptions;
    return make_shared<BatchConsumeBudgetCommandOverride>(
        transaction_id, budget_key, move(budget_consumptions_copy));
  }

  shared_ptr<ConsumeBudgetCommandOverride> GetConsumeBudgetCommandOverride(
      core::common::Uuid& transaction_id, shared_ptr<string> budget_key,
      const ConsumeBudgetCommandRequestInfo& budget_consumption) {
    auto budget_consumption_copy = budget_consumption;
    return make_shared<ConsumeBudgetCommandOverride>(
        transaction_id, budget_key, move(budget_consumption_copy));
  }

  static pair<vector<ConsumeBudgetCommandRequestInfo>,
              vector<ConsumeBudgetCommandRequestInfo>>
  GetBatchBudgetConsumptions_Sample1() {
    vector<ConsumeBudgetCommandRequestInfo> budget_consumptions(
        {{hours(1).count(), 1, 1},
         {hours(2).count(), 2, 2},
         {hours(3).count(), 3, 3},
         {hours(4).count(), 4, 4},
         {hours(5).count(), 5, 5}});
    vector<ConsumeBudgetCommandRequestInfo> failed_budget_consumptions(
        {{hours(1).count(), 1, 1},
         {hours(4).count(), 4, 4},
         {hours(5).count(), 5, 5}});
    return make_pair(budget_consumptions, failed_budget_consumptions);
  }

  static pair<vector<ConsumeBudgetCommandRequestInfo>,
              vector<ConsumeBudgetCommandRequestInfo>>
  GetBatchBudgetConsumptions_Sample2() {
    vector<ConsumeBudgetCommandRequestInfo> budget_consumptions{
        {hours(6).count(), 6, 6},
        {hours(7).count(), 7, 7},
        {hours(8).count(), 8, 8},
        {hours(9).count(), 9, 9},
        {hours(10).count(), 10, 10}};
    vector<ConsumeBudgetCommandRequestInfo> failed_budget_consumptions(
        {{hours(8).count(), 8, 8},
         {hours(9).count(), 9, 9},
         {hours(10).count(), 10, 10}});
    return make_pair(budget_consumptions, failed_budget_consumptions);
  }

  ConsumeBudgetCommandRequestInfo GetBudgetConsumption_Sample() {
    return {hours(20).count(), 20, 11};
  }

  static string GetBeginTransactionHttpRequestBody_Sample() {
    return R"({
        "v": "1.0",
        "t": [
            {
                "key": "test_key",
                "token": 10,
                "reporting_time": "2019-10-12T07:20:50.52Z"
            },
            {
                "key": "test_key_2",
                "token": 23,
                "reporting_time": "2019-12-12T07:20:50.52Z"
            }
        ]
    })";
  }

  static AsyncContext<HttpRequest, HttpResponse>
  GetBeginTransactionHttpRequestContext_Sample() {
    AsyncContext<HttpRequest, HttpResponse> http_context;
    http_context.request = make_shared<HttpRequest>();
    http_context.request->headers = make_shared<HttpHeaders>();
    http_context.request->headers->insert(
        {string(kTransactionIdHeader), "3E2A3D09-48ED-A355-D346-AD7DC6CB0909"});
    http_context.request->headers->insert(
        {string(kTransactionSecretHeader), "this_is_a_secret"});
    http_context.request->auth_context.authorized_domain =
        make_shared<string>("foo.com");
    auto begin_transaction_body_string =
        GetBeginTransactionHttpRequestBody_Sample();
    http_context.request->body.bytes = make_shared<vector<Byte>>();
    http_context.request->body.bytes =
        make_shared<vector<Byte>>(begin_transaction_body_string.begin(),
                                  begin_transaction_body_string.end());
    http_context.request->body.capacity =
        begin_transaction_body_string.length();
    http_context.request->body.length = begin_transaction_body_string.length();
    return http_context;
  }

  // Dependent components
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<TransactionManagerInterface> transaction_manager_;
  shared_ptr<MockFrontEndServiceWithOverrides> front_end_service_;
  shared_ptr<MockConfigProvider> mock_config_provider_;
  MockTransactionRequestRouter* mock_transaction_request_router_;

  AsyncContext<TransactionRequest, TransactionResponse> transaction_context_;
};

struct TestCase {
  std::string test_name;
  bool enable_per_site_enrollment;
};

class FrontEndServiceTestWithParam
    : public FrontEndServiceTest,
      public testing::WithParamInterface<TestCase> {
  void SetUp() override {
    setenv(core::kPBSAuthorizationEnableSiteBasedAuthorization,
           GetParam().enable_per_site_enrollment ? "true" : "false",
           /*replace=*/1);
    FrontEndServiceTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    FrontEndServiceTestWithParam, FrontEndServiceTestWithParam,
    testing::Values(TestCase{"EnablePerSiteEnrollment", true},
                    TestCase{"DisablePerSiteEnrollment", false}),
    [](const testing::TestParamInfo<FrontEndServiceTestWithParam::ParamType>&
           info) { return info.param.test_name; });

TEST_P(FrontEndServiceTestWithParam,
       ExecuteConsumeBudgetOperationInvalidRequest) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;
  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();

  FrontEndService front_end_service(http2_server, mock_async_executor,
                                    std::move(mock_transaction_request_router),
                                    std::move(consume_budget_command_factory),
                                    mock_metric_client, mock_config_provider);
  ConsumeBudgetTransactionRequest consume_budget_transaction_request;
  consume_budget_transaction_request.budget_keys =
      make_shared<vector<ConsumeBudgetMetadata>>();
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context(
          make_shared<ConsumeBudgetTransactionRequest>(
              consume_budget_transaction_request),
          [](AsyncContext<ConsumeBudgetTransactionRequest,
                          ConsumeBudgetTransactionResponse>&
                 consume_budget_transaction_context) {});

  EXPECT_EQ(front_end_service.ExecuteConsumeBudgetTransaction(
                consume_budget_transaction_context),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST));
}

TEST_P(FrontEndServiceTestWithParam,
       ExecuteConsumeBudgetOperationTransactionManagerFailure) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    auto mock_transaction_request_router = GetMockTransactionRequestRouter();
    EXPECT_CALL(
        *mock_transaction_request_router,
        Execute(An<AsyncContext<TransactionRequest, TransactionResponse>&>()))
        .WillOnce(
            [result](AsyncContext<TransactionRequest, TransactionResponse>&) {
              return result;
            });
    shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
        make_shared<MockNoSQLDatabaseProvider>();
    shared_ptr<HttpServerInterface> http2_server =
        make_shared<MockHttp2Server>();
    std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
        consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();

    FrontEndService front_end_service(
        http2_server, mock_async_executor,
        std::move(mock_transaction_request_router),
        std::move(consume_budget_command_factory), mock_metric_client,
        mock_config_provider);

    ConsumeBudgetTransactionRequest consume_budget_transaction_request;
    consume_budget_transaction_request.budget_keys =
        make_shared<vector<ConsumeBudgetMetadata>>();
    consume_budget_transaction_request.budget_keys->push_back({});

    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>
        consume_budget_transaction_context(
            make_shared<ConsumeBudgetTransactionRequest>(
                consume_budget_transaction_request),
            [](AsyncContext<ConsumeBudgetTransactionRequest,
                            ConsumeBudgetTransactionResponse>&
                   consume_budget_transaction_context) {});

    EXPECT_EQ(front_end_service.ExecuteConsumeBudgetTransaction(
                  consume_budget_transaction_context),
              result);
  }
}

TEST_P(FrontEndServiceTestWithParam,
       ExecuteConsumeBudgetOperationCommandConstruction) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  EXPECT_CALL(
      *mock_transaction_request_router,
      Execute(An<AsyncContext<TransactionRequest, TransactionResponse>&>()))
      .WillOnce([](AsyncContext<TransactionRequest, TransactionResponse>&
                       transaction_context) {
        EXPECT_EQ(transaction_context.request->commands.size(), 100);
        EXPECT_NE(transaction_context.request->timeout_time, 0);
        EXPECT_NE(transaction_context.request->transaction_id.high, 0);
        EXPECT_NE(transaction_context.request->transaction_id.low, 0);
        return SuccessExecutionResult();
      });

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();

  FrontEndService front_end_service(http2_server, mock_async_executor,
                                    std::move(mock_transaction_request_router),
                                    std::move(consume_budget_command_factory),
                                    mock_metric_client, mock_config_provider);
  ConsumeBudgetTransactionRequest consume_budget_transaction_request;
  consume_budget_transaction_request.budget_keys =
      make_shared<vector<ConsumeBudgetMetadata>>();

  for (int i = 0; i < 100; i++) {
    ConsumeBudgetMetadata consume_budget_metadata;
    consume_budget_metadata.budget_key_name = make_shared<BudgetKeyName>();
    consume_budget_metadata.time_bucket = i;
    consume_budget_metadata.token_count = i * 100;
    consume_budget_transaction_request.budget_keys->push_back(
        move(consume_budget_metadata));
  }

  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context(
          make_shared<ConsumeBudgetTransactionRequest>(
              consume_budget_transaction_request),
          [](AsyncContext<ConsumeBudgetTransactionRequest,
                          ConsumeBudgetTransactionResponse>&
                 consume_budget_transaction_context) {});

  EXPECT_SUCCESS(front_end_service.ExecuteConsumeBudgetTransaction(
      consume_budget_transaction_context));
}

TEST_P(FrontEndServiceTestWithParam,
       ExecuteConsumeBudgetOperationTransactionResults) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    auto mock_transaction_request_router = GetMockTransactionRequestRouter();
    EXPECT_CALL(
        *mock_transaction_request_router,
        Execute(An<AsyncContext<TransactionRequest, TransactionResponse>&>()))
        .WillOnce(
            [result](AsyncContext<TransactionRequest, TransactionResponse>&
                         transaction_context) {
              transaction_context.result = result;
              transaction_context.Finish();
              return SuccessExecutionResult();
            });

    std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
        consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
    shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
        make_shared<MockNoSQLDatabaseProvider>();

    shared_ptr<HttpServerInterface> http2_server =
        make_shared<MockHttp2Server>();

    FrontEndService front_end_service(
        http2_server, mock_async_executor,
        std::move(mock_transaction_request_router),
        std::move(consume_budget_command_factory), mock_metric_client,
        mock_config_provider);
    ConsumeBudgetTransactionRequest consume_budget_transaction_request;
    consume_budget_transaction_request.budget_keys =
        make_shared<vector<ConsumeBudgetMetadata>>();
    consume_budget_transaction_request.budget_keys->push_back({});

    atomic<bool> condition = false;
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>
        consume_budget_transaction_context(
            make_shared<ConsumeBudgetTransactionRequest>(
                consume_budget_transaction_request),
            [&](AsyncContext<ConsumeBudgetTransactionRequest,
                             ConsumeBudgetTransactionResponse>&
                    consume_budget_transaction_context) {
              EXPECT_THAT(consume_budget_transaction_context.result,
                          ResultIs(result));
              condition = true;
            });

    EXPECT_EQ(front_end_service.ExecuteConsumeBudgetTransaction(
                  consume_budget_transaction_context),
              SuccessExecutionResult());
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(FrontEndServiceTestWithParam,
       BeginTransactionFailsIfNewTransactionsAreDisallowed) {
  auto begin_transaction_context =
      GetBeginTransactionHttpRequestContext_Sample();

  mock_config_provider_->SetBool(pbs::kDisallowNewTransactionRequests, true);

  front_end_service_->InitMetricInstances();
  EXPECT_EQ(
      front_end_service_->BeginTransaction(begin_transaction_context),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_BEGIN_TRANSACTION_DISALLOWED));

  mock_config_provider_->SetBool(pbs::kDisallowNewTransactionRequests, false);

  // Set the execute transaction mock for the BeginTransaction
  EXPECT_CALL(
      *mock_transaction_request_router_,
      Execute(An<AsyncContext<TransactionRequest, TransactionResponse>&>()))
      .WillOnce(Return(FailureExecutionResult(12345)));

  EXPECT_THAT(front_end_service_->BeginTransaction(begin_transaction_context),
              ResultIs(FailureExecutionResult(12345)));
}

TEST_P(FrontEndServiceTestWithParam, BeginTransactionInvalidBody) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;
  auto mock_transaction_request_router = GetMockTransactionRequestRouter();

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();

  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);

  front_end_service.InitMetricInstances();
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->body.bytes = make_shared<vector<Byte>>();
  http_context.request->headers = make_shared<HttpHeaders>();
  EXPECT_EQ(
      front_end_service.BeginTransaction(http_context),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  auto total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelBeginTransaction, kMetricNameTotalRequest);
  auto client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelBeginTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);

  http_context.request->headers->insert(
      {string(kTransactionIdHeader), "3E2A3D09-48ED-A355-D346-AD7DC6CB0909"});
  EXPECT_EQ(
      front_end_service.BeginTransaction(http_context),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 2);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 2);

  http_context.request->headers->insert(
      {string(kTransactionSecretHeader), "this_is_a_secret"});
  EXPECT_EQ(front_end_service.BeginTransaction(http_context),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 3);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 3);
}

TEST_P(FrontEndServiceTestWithParam, BeginTransactionValidBody) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  std::shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  atomic<bool> condition = false;

  EXPECT_CALL(
      *mock_transaction_request_router,
      Execute(An<AsyncContext<TransactionRequest, TransactionResponse>&>()))
      .WillOnce([&](AsyncContext<TransactionRequest, TransactionResponse>&
                        transaction_context) {
        EXPECT_EQ(transaction_context.request->commands.size(), 2);
        EXPECT_NE(transaction_context.request->timeout_time, 0);
        EXPECT_NE(transaction_context.request->transaction_id.high, 0);
        EXPECT_NE(transaction_context.request->transaction_id.low, 0);
        EXPECT_EQ(*transaction_context.request->transaction_secret,
                  "transaction_secret");
        EXPECT_EQ(*transaction_context.request->transaction_origin, "foo.com");

        auto command = static_pointer_cast<ConsumeBudgetCommand>(
            transaction_context.request->commands[0]);
        EXPECT_EQ(*command->GetBudgetKeyName(), "foo.com/test_key");
        EXPECT_EQ(command->GetTimeBucket(), 1570864850000000000);
        EXPECT_EQ(command->GetTokenCount(), 10);

        command = static_pointer_cast<ConsumeBudgetCommand>(
            transaction_context.request->commands[1]);
        EXPECT_EQ(*command->GetBudgetKeyName(), "foo.com/test_key_2");
        EXPECT_EQ(command->GetTimeBucket(), 1576135250000000000);
        EXPECT_EQ(command->GetTokenCount(), 23);
        condition = true;
        return SuccessExecutionResult();
      });

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();

  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);

  front_end_service.InitMetricInstances();
  string begin_transaction_body_string =
      GetBeginTransactionHttpRequestBody_Sample();
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes =
      make_shared<vector<Byte>>(begin_transaction_body_string.begin(),
                                begin_transaction_body_string.end());
  bytes_buffer.capacity = begin_transaction_body_string.length();
  bytes_buffer.length = begin_transaction_body_string.length();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->body = bytes_buffer;
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->auth_context.authorized_domain =
      make_shared<string>("foo.com");
  EXPECT_EQ(
      front_end_service.BeginTransaction(http_context),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  auto total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelBeginTransaction, kMetricNameTotalRequest);
  auto client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelBeginTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);

  http_context.request->headers->insert(
      {string(kTransactionIdHeader), "3E2A3D09-48ED-A355-D346-AD7DC6CB0909"});
  http_context.request->headers->insert(
      {string(kTransactionSecretHeader), string("transaction_secret")});
  EXPECT_EQ(front_end_service.BeginTransaction(http_context),
            SuccessExecutionResult());
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 2);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(FrontEndServiceTestWithParam, OnTransactionCallbackFailed) {
  atomic<bool> condition = false;
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.response = make_shared<HttpResponse>();
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        EXPECT_THAT(http_context.result, ResultIs(FailureExecutionResult(123)));
        condition = true;
      };

  auto mock_metric_transaction = make_shared<MockAggregateMetric>();
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.response = make_shared<TransactionResponse>();
  transaction_context.result = FailureExecutionResult(123);

  front_end_service_->OnTransactionCallback(mock_metric_transaction,
                                            http_context, transaction_context);
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
            0);
}

TEST_P(FrontEndServiceTestWithParam, OnTransactionCallback) {
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context;
  transaction_context.request = make_shared<TransactionRequest>();
  transaction_context.request->transaction_id = Uuid::GenerateUuid();
  transaction_context.request->transaction_secret =
      make_shared<string>("secret");
  transaction_context.request->transaction_origin =
      make_shared<string>("origin");

  transaction_context.result = SuccessExecutionResult();
  transaction_context.response = make_shared<TransactionResponse>();
  transaction_context.response->transaction_id =
      transaction_context.request->transaction_id;
  transaction_context.response->failed_commands_indices = {1, 2, 3, 4, 5};
  transaction_context.response->last_execution_timestamp = 1234567;

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(123)};
  vector<size_t> expected_server_error_metrics = {0, 1, 1};

  for (int i = 0; i < results.size(); i++) {
    auto result = results[i];
    atomic<bool> condition = false;
    AsyncContext<HttpRequest, HttpResponse> http_context;
    http_context.response = make_shared<HttpResponse>();
    http_context.request = make_shared<HttpRequest>();
    http_context.request->headers = make_shared<HttpHeaders>();
    http_context.request->auth_context.authorized_domain =
        make_shared<string>("origin");
    http_context.response->headers = make_shared<core::HttpHeaders>();
    http_context.callback =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          EXPECT_THAT(http_context.result, ResultIs(result));

          if (result.status != core::ExecutionStatus::Failure) {
            EXPECT_EQ(http_context.response->body.capacity, 0);
            EXPECT_EQ(http_context.response->body.length, 0);
          } else {
            string body(http_context.response->body.bytes->begin(),
                        http_context.response->body.bytes->end());
            EXPECT_EQ(body, R"({"f":[1,2,3,4,5],"v":"1.0"})");
            EXPECT_EQ(http_context.response->body.length, body.length());
            EXPECT_EQ(http_context.response->body.capacity, body.length());
          }

          condition = true;
        };

    front_end_service_->execution_transaction_phase_mock =
        [&](const shared_ptr<AggregateMetricInterface>& metric_instance,
            AsyncContext<HttpRequest, HttpResponse>& http_context,
            Uuid& transaction_id, shared_ptr<string>& transaction_secret,
            shared_ptr<string>& transaction_origin,
            Timestamp transaction_last_execution_timestamp,
            TransactionExecutionPhase transaction_phase) {
          EXPECT_EQ(transaction_context.request->transaction_id,
                    transaction_id);
          EXPECT_EQ(transaction_phase, TransactionExecutionPhase::Begin);
          EXPECT_EQ(transaction_last_execution_timestamp, 1234567);
          EXPECT_EQ(*transaction_secret, "secret");
          EXPECT_EQ(*transaction_origin, "origin");
          condition = result.Successful();
          return result;
        };

    transaction_context.result = result;
    auto mock_metric_transaction = make_shared<MockAggregateMetric>();
    front_end_service_->OnTransactionCallback(
        mock_metric_transaction, http_context, transaction_context);
    WaitUntil([&]() { return condition.load(); });
    size_t expected_server_error_metric = expected_server_error_metrics[i];
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator),
              expected_server_error_metric);
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
              0);
  }
}

TEST_P(FrontEndServiceTestWithParam, OnTransactionCallbackWithBatchCommands) {
  // Create Batch
  auto batch_budgets1 = GetBatchBudgetConsumptions_Sample1();
  auto batch_command1 = GetBatchConsumeBudgetCommandOverride(
      transaction_context_.request->transaction_id, make_shared<string>("key1"),
      batch_budgets1.first);
  batch_command1->SetFailedBudgetsWithInsufficientConsumption(
      batch_budgets1.second);

  // Create Batch
  auto batch_budgets2 = GetBatchBudgetConsumptions_Sample2();
  auto batch_command2 = GetBatchConsumeBudgetCommandOverride(
      transaction_context_.request->transaction_id, make_shared<string>("key2"),
      batch_budgets2.first);
  batch_command2->SetFailedBudgetsWithInsufficientConsumption(
      batch_budgets2.second);

  // Create Non-Batch
  auto non_batch_command = GetConsumeBudgetCommandOverride(
      transaction_context_.request->transaction_id, make_shared<string>("key3"),
      GetBudgetConsumption_Sample());
  non_batch_command->SetBudgetFailedDueToInsufficientConsumption();

  // All of the commands have failed
  transaction_context_.response->failed_commands_indices = {1, 2, 3};
  transaction_context_.response->failed_commands = {
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(batch_command1),
      dynamic_pointer_cast<ConsumeBudgetCommand>(non_batch_command),
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(batch_command2)};
  transaction_context_.response->last_execution_timestamp = 1234567;

  // String based on the 3 command's failed indices above.
  auto failed_indicies_expected_string = R"({"f":[1,4,5,8,9,10,11],"v":"1.0"})";

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(123)};
  vector<size_t> expected_server_error_metrics = {0, 1, 1};

  for (int i = 0; i < results.size(); i++) {
    auto result = results[i];
    atomic<bool> condition = false;
    AsyncContext<HttpRequest, HttpResponse> http_context;
    http_context.response = make_shared<HttpResponse>();
    http_context.request = make_shared<HttpRequest>();
    http_context.request->headers = make_shared<HttpHeaders>();
    http_context.request->auth_context.authorized_domain =
        make_shared<string>("origin");
    http_context.response->headers = make_shared<core::HttpHeaders>();
    http_context.callback =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          EXPECT_THAT(http_context.result, ResultIs(result));

          if (result.status != core::ExecutionStatus::Failure) {
            EXPECT_EQ(http_context.response->body.capacity, 0);
            EXPECT_EQ(http_context.response->body.length, 0);
          } else {
            string body(http_context.response->body.bytes->begin(),
                        http_context.response->body.bytes->end());
            EXPECT_EQ(body, failed_indicies_expected_string);
            EXPECT_EQ(http_context.response->body.length, body.length());
            EXPECT_EQ(http_context.response->body.capacity, body.length());
          }

          condition = true;
        };

    front_end_service_->execution_transaction_phase_mock =
        [&](const shared_ptr<AggregateMetricInterface>& metric_instance,
            AsyncContext<HttpRequest, HttpResponse>& http_context,
            Uuid& transaction_id, shared_ptr<string>& transaction_secret,
            shared_ptr<string>& transaction_origin,
            Timestamp transaction_last_execution_timestamp,
            TransactionExecutionPhase transaction_phase) {
          EXPECT_EQ(transaction_context_.request->transaction_id,
                    transaction_id);
          EXPECT_EQ(transaction_phase, TransactionExecutionPhase::Begin);
          EXPECT_EQ(transaction_last_execution_timestamp, 1234567);
          EXPECT_EQ(*transaction_secret, "secret");
          EXPECT_EQ(*transaction_origin, "origin");

          if (result.Successful()) {
            condition = true;
          }
          return result;
        };

    transaction_context_.result = result;
    auto mock_metric_transaction = make_shared<MockAggregateMetric>();
    front_end_service_->OnTransactionCallback(
        mock_metric_transaction, http_context, transaction_context_);
    WaitUntil([&]() { return condition.load(); });
    size_t expected_server_error_metric = expected_server_error_metrics[i];
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator),
              expected_server_error_metric);
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
              0);
  }
}

TEST_P(FrontEndServiceTestWithParam,
       ObtainTransactionOriginReturnsAuthorizedDomain) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->auth_context.authorized_domain =
      make_shared<string>("origin");
  http_context.response->headers = make_shared<core::HttpHeaders>();
  EXPECT_EQ(*front_end_service_->ObtainTransactionOrigin(http_context),
            "origin");
}

TEST_P(FrontEndServiceTestWithParam,
       ObtainTransactionOriginReturnsHeaderValue) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->auth_context.authorized_domain =
      make_shared<string>("origin");
  auto origin_header =
      make_pair(string(kTransactionOriginHeader), string("origin from header"));
  http_context.request->headers->insert(origin_header);

  EXPECT_EQ(*front_end_service_->ObtainTransactionOrigin(http_context),
            "origin from header");
}

TEST_P(FrontEndServiceTestWithParam, InvalidTransactionId) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);

  front_end_service.InitMetricInstances();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<core::HttpHeaders>();
  string value;
  auto pair =
      make_pair(string(kTransactionIdHeader), string("dddddddd-ddddddd"));
  http_context.request->headers->insert(pair);

  EXPECT_EQ(front_end_service.PrepareTransaction(http_context),
            FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING));
  auto total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelPrepareTransaction, kMetricNameTotalRequest);
  auto client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelPrepareTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);

  EXPECT_EQ(front_end_service.CommitTransaction(http_context),
            FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING));
  total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelCommitTransaction, kMetricNameTotalRequest);
  client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelCommitTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);

  EXPECT_EQ(front_end_service.NotifyTransaction(http_context),
            FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING));
  total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelNotifyTransaction, kMetricNameTotalRequest);
  client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelNotifyTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);

  EXPECT_EQ(front_end_service.AbortTransaction(http_context),
            FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING));
  total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelAbortTransaction, kMetricNameTotalRequest);
  client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelAbortTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);

  EXPECT_EQ(front_end_service.EndTransaction(http_context),
            FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING));
  total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelEndTransaction, kMetricNameTotalRequest);
  client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelEndTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 1);
}

TEST_P(FrontEndServiceTestWithParam, ValidTransactionNotValidPhase) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  EXPECT_CALL(*mock_transaction_request_router,
              Execute(An<AsyncContext<TransactionPhaseRequest,
                                      TransactionPhaseResponse>&>()))
      .Times(5)
      .WillRepeatedly(Return(FailureExecutionResult(123)));

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);
  front_end_service.InitMetricInstances();
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<core::HttpHeaders>();
  string value;
  auto transaction_id = string("3E2A3D09-48ED-A355-D346-AD7DC6CB0909");
  auto pair = make_pair(string(kTransactionIdHeader), transaction_id);
  http_context.request->headers->insert(pair);
  http_context.request->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader), "12345678"});
  http_context.request->headers->insert(
      {string(kTransactionSecretHeader), "this_is_a_secret"});

  EXPECT_THAT(front_end_service.PrepareTransaction(http_context),
              ResultIs(FailureExecutionResult(123)));
  EXPECT_THAT(front_end_service.CommitTransaction(http_context),
              ResultIs(FailureExecutionResult(123)));
  EXPECT_THAT(front_end_service.NotifyTransaction(http_context),
              ResultIs(FailureExecutionResult(123)));
  EXPECT_THAT(front_end_service.AbortTransaction(http_context),
              ResultIs(FailureExecutionResult(123)));
  EXPECT_THAT(front_end_service.EndTransaction(http_context),
              ResultIs(FailureExecutionResult(123)));
}

TEST_P(FrontEndServiceTestWithParam, ValidTransactionValidPhase) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  auto mock_transaction_request_router_copy =
      mock_transaction_request_router.get();

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);
  front_end_service.InitMetricInstances();
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<core::HttpHeaders>();
  string value;
  auto transaction_id = string("3E2A3D09-48ED-A355-D346-AD7DC6CB0909");
  auto pair = make_pair(string(kTransactionIdHeader), transaction_id);
  http_context.request->headers->insert(pair);
  http_context.request->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader), "12345678"});
  http_context.request->headers->insert(
      {string(kTransactionSecretHeader), "this_is_a_secret"});

  Uuid uuid;
  core::common::FromString(transaction_id, uuid);

  EXPECT_CALL(*mock_transaction_request_router_copy,
              Execute(An<AsyncContext<TransactionPhaseRequest,
                                      TransactionPhaseResponse>&>()))
      .WillOnce(
          [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_EQ(transaction_phase_context.request->transaction_id, uuid);
            EXPECT_EQ(
                transaction_phase_context.request->transaction_execution_phase,
                TransactionExecutionPhase::Prepare);
            return SuccessExecutionResult();
          });

  EXPECT_EQ(front_end_service.PrepareTransaction(http_context),
            SuccessExecutionResult());

  EXPECT_CALL(*mock_transaction_request_router_copy,
              Execute(An<AsyncContext<TransactionPhaseRequest,
                                      TransactionPhaseResponse>&>()))
      .WillOnce(
          [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_EQ(transaction_phase_context.request->transaction_id, uuid);
            EXPECT_EQ(
                transaction_phase_context.request->transaction_execution_phase,
                TransactionExecutionPhase::Commit);
            return SuccessExecutionResult();
          });
  EXPECT_EQ(front_end_service.CommitTransaction(http_context),
            SuccessExecutionResult());

  EXPECT_CALL(*mock_transaction_request_router_copy,
              Execute(An<AsyncContext<TransactionPhaseRequest,
                                      TransactionPhaseResponse>&>()))
      .WillOnce(
          [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_EQ(transaction_phase_context.request->transaction_id, uuid);
            EXPECT_EQ(
                transaction_phase_context.request->transaction_execution_phase,
                TransactionExecutionPhase::Notify);
            return SuccessExecutionResult();
          });
  EXPECT_EQ(front_end_service.NotifyTransaction(http_context),
            SuccessExecutionResult());

  EXPECT_CALL(*mock_transaction_request_router_copy,
              Execute(An<AsyncContext<TransactionPhaseRequest,
                                      TransactionPhaseResponse>&>()))
      .WillOnce(
          [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_EQ(transaction_phase_context.request->transaction_id, uuid);
            EXPECT_EQ(
                transaction_phase_context.request->transaction_execution_phase,
                TransactionExecutionPhase::Abort);
            return SuccessExecutionResult();
          });
  EXPECT_EQ(front_end_service.AbortTransaction(http_context),
            SuccessExecutionResult());

  EXPECT_CALL(*mock_transaction_request_router_copy,
              Execute(An<AsyncContext<TransactionPhaseRequest,
                                      TransactionPhaseResponse>&>()))
      .WillOnce(
          [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  transaction_phase_context) {
            EXPECT_EQ(transaction_phase_context.request->transaction_id, uuid);
            EXPECT_EQ(
                transaction_phase_context.request->transaction_execution_phase,
                TransactionExecutionPhase::End);
            return SuccessExecutionResult();
          });
  EXPECT_EQ(front_end_service.EndTransaction(http_context),
            SuccessExecutionResult());
}

TEST_P(FrontEndServiceTestWithParam, OnExecuteTransactionPhaseCallback) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  vector<size_t> expected_server_error_metrics = {0, 1, 1};

  for (int i = 0; i < results.size(); i++) {
    auto result = results[i];
    atomic<bool> condition = false;
    AsyncContext<HttpRequest, HttpResponse> http_context;
    http_context.request = make_shared<HttpRequest>();
    http_context.request->headers = make_shared<HttpHeaders>();
    http_context.response = make_shared<HttpResponse>();
    http_context.response->headers = make_shared<HttpHeaders>();
    http_context.callback =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          EXPECT_THAT(http_context.result, ResultIs(result));
          condition = true;
        };

    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
        transaction_phase_context;
    transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
    transaction_phase_context.result = result;
    transaction_phase_context.response =
        make_shared<TransactionPhaseResponse>();

    auto mock_metric_transaction = make_shared<MockAggregateMetric>();
    front_end_service.OnExecuteTransactionPhaseCallback(
        mock_metric_transaction, http_context, transaction_phase_context);
    WaitUntil([&]() { return condition.load(); });
    size_t expected_server_error_metric = expected_server_error_metrics[i];
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator),
              expected_server_error_metric);
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
              0);
  }
}

TEST_P(FrontEndServiceTestWithParam,
       OnExecuteTransactionPhaseCallbackFailureWithKeys) {
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};
  vector<size_t> expected_server_error_metrics = {0, 1, 1};

  for (int i = 0; i < results.size(); i++) {
    auto result = results[i];
    atomic<bool> condition = false;
    AsyncContext<HttpRequest, HttpResponse> http_context;
    http_context.request = make_shared<HttpRequest>();
    http_context.request->headers = make_shared<HttpHeaders>();
    http_context.response = make_shared<HttpResponse>();
    http_context.response->headers = make_shared<HttpHeaders>();
    http_context.callback =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          EXPECT_THAT(http_context.result, ResultIs(result));

          if (result.status != core::ExecutionStatus::Failure) {
            EXPECT_EQ(http_context.response->body.capacity, 0);
            EXPECT_EQ(http_context.response->body.length, 0);
          } else {
            string response(http_context.response->body.bytes->begin(),
                            http_context.response->body.bytes->end());
            EXPECT_EQ(response, R"({"f":[1,2,3],"v":"1.0"})");
            EXPECT_EQ(http_context.response->body.length, response.length());
            EXPECT_EQ(http_context.response->body.capacity, response.length());
          }
          condition = true;
        };

    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
        transaction_phase_context;
    transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
    transaction_phase_context.result = result;
    transaction_phase_context.response =
        make_shared<TransactionPhaseResponse>();

    transaction_phase_context.response->failed_commands_indices.push_back(1);
    transaction_phase_context.response->failed_commands_indices.push_back(2);
    transaction_phase_context.response->failed_commands_indices.push_back(3);

    auto mock_metric_transaction = make_shared<MockAggregateMetric>();
    front_end_service_->OnExecuteTransactionPhaseCallback(
        mock_metric_transaction, http_context, transaction_phase_context);
    WaitUntil([&]() { return condition.load(); });
    size_t expected_server_error_metric = expected_server_error_metrics[i];
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator),
              expected_server_error_metric);
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
              0);
  }
}

TEST_P(FrontEndServiceTestWithParam,
       OnExecuteTransactionPhaseCallbackFailureWithBatchCommands) {
  // Create Batch
  auto batch_budgets1 = GetBatchBudgetConsumptions_Sample1();
  auto batch_command1 = GetBatchConsumeBudgetCommandOverride(
      transaction_context_.request->transaction_id, make_shared<string>("key1"),
      batch_budgets1.first);
  batch_command1->SetFailedBudgetsWithInsufficientConsumption(
      batch_budgets1.second);

  // Create Batch
  auto batch_budgets2 = GetBatchBudgetConsumptions_Sample2();
  auto batch_command2 = GetBatchConsumeBudgetCommandOverride(
      transaction_context_.request->transaction_id, make_shared<string>("key2"),
      batch_budgets2.first);
  batch_command2->SetFailedBudgetsWithInsufficientConsumption(
      batch_budgets2.second);

  // Create Non-Batch
  auto non_batch_command = GetConsumeBudgetCommandOverride(
      transaction_context_.request->transaction_id, make_shared<string>("key3"),
      GetBudgetConsumption_Sample());
  non_batch_command->SetBudgetFailedDueToInsufficientConsumption();

  // String based on the 3 command's failed indices above.
  auto failed_indicies_expected_string = R"({"f":[1,4,5,8,9,10,11],"v":"1.0"})";

  // Test
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};
  vector<size_t> expected_server_error_metrics = {0, 1, 1};

  for (int i = 0; i < results.size(); i++) {
    auto result = results[i];
    atomic<bool> condition = false;
    AsyncContext<HttpRequest, HttpResponse> http_context;
    http_context.request = make_shared<HttpRequest>();
    http_context.request->headers = make_shared<HttpHeaders>();
    http_context.response = make_shared<HttpResponse>();
    http_context.response->headers = make_shared<HttpHeaders>();
    http_context.callback =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          EXPECT_THAT(http_context.result, ResultIs(result));

          if (result.status != core::ExecutionStatus::Failure) {
            EXPECT_EQ(http_context.response->body.capacity, 0);
            EXPECT_EQ(http_context.response->body.length, 0);
          } else {
            string response(http_context.response->body.bytes->begin(),
                            http_context.response->body.bytes->end());
            EXPECT_EQ(response, failed_indicies_expected_string);
            EXPECT_EQ(http_context.response->body.length, response.length());
            EXPECT_EQ(http_context.response->body.capacity, response.length());
          }
          condition = true;
        };

    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
        transaction_phase_context;
    transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
    transaction_phase_context.result = result;
    transaction_phase_context.response =
        make_shared<TransactionPhaseResponse>();

    transaction_phase_context.response->failed_commands_indices = {1, 2, 3};
    transaction_phase_context.response->failed_commands = {
        batch_command1, non_batch_command, batch_command2};

    auto mock_metric_transaction = make_shared<MockAggregateMetric>();
    front_end_service_->OnExecuteTransactionPhaseCallback(
        mock_metric_transaction, http_context, transaction_phase_context);
    WaitUntil([&]() { return condition.load(); });
    size_t expected_server_error_metric = expected_server_error_metrics[i];
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator),
              expected_server_error_metric);
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
              0);
  }
}

TEST_P(FrontEndServiceTestWithParam, GetServiceStatus) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.request = make_shared<HttpRequest>();
  atomic<bool> callback_invoked(false);

  string expected_body = "{\"pending_transactions_count\":19,\"v\":\"1.0\"}";

  http_context.response->headers = make_shared<core::HttpHeaders>();
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        EXPECT_SUCCESS(http_context.result);
        string body(http_context.response->body.bytes->begin(),
                    http_context.response->body.bytes->end());
        EXPECT_EQ(body, expected_body);
        EXPECT_EQ(http_context.response->body.length, body.length());
        EXPECT_EQ(http_context.response->body.capacity, body.length());

        callback_invoked = true;
      };

  EXPECT_CALL(*mock_transaction_request_router_,
              Execute(An<const GetTransactionManagerStatusRequest&>(),
                      An<GetTransactionManagerStatusResponse&>()))
      .WillOnce([&](const GetTransactionManagerStatusRequest&,
                    GetTransactionManagerStatusResponse& response) {
        response.pending_transactions_count = 19;
        return SuccessExecutionResult();
      });

  EXPECT_EQ(front_end_service_->GetServiceStatus(http_context),
            SuccessExecutionResult());

  WaitUntil([&]() { return callback_invoked.load(); });
}

TEST_P(FrontEndServiceTestWithParam, GetServiceStatusFailure) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.request = make_shared<HttpRequest>();
  atomic<bool> callback_invoked(false);

  http_context.response->headers = make_shared<core::HttpHeaders>();
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        EXPECT_THAT(http_context.result,
                    ResultIs(FailureExecutionResult(1234)));
        callback_invoked = true;
      };

  EXPECT_CALL(*mock_transaction_request_router_,
              Execute(An<const GetTransactionManagerStatusRequest&>(),
                      An<GetTransactionManagerStatusResponse&>()))
      .WillOnce([&](const GetTransactionManagerStatusRequest&,
                    GetTransactionManagerStatusResponse& response) {
        response.pending_transactions_count = 19;
        return FailureExecutionResult(1234);
      });

  EXPECT_EQ(front_end_service_->GetServiceStatus(http_context),
            FailureExecutionResult(1234));
  // callback is not invoked as the failure is conveyed synchronously
  EXPECT_EQ(callback_invoked, false);
}

TEST_P(FrontEndServiceTestWithParam, GetTransactionStatus) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  auto mock_transaction_request_router_copy =
      mock_transaction_request_router.get();

  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  atomic<bool> condition = false;

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);

  front_end_service.InitMetricInstances();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<HttpHeaders>();
  auto claimed_identity = make_pair(string("x-gscp-claimed-identity"),
                                    string("remote-coordinator.com"));
  http_context.request->headers->insert(claimed_identity);
  EXPECT_EQ(
      front_end_service.GetTransactionStatus(http_context),
      core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  auto total_request_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelGetStatusTransaction, kMetricNameTotalRequest);
  auto client_errors_metric_instance = front_end_service.GetMetricsInstance(
      kMetricLabelGetStatusTransaction, kMetricNameClientError);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueCoordinator),
      1);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueCoordinator),
      1);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 0);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 0);

  auto pair = make_pair(string(kTransactionIdHeader),
                        string("3E2A3D09-48ED-A355-D346-AD7DC6CB0909"));
  http_context.request->headers->insert(pair);

  EXPECT_EQ(
      front_end_service.GetTransactionStatus(http_context),
      core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueCoordinator),
      2);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueCoordinator),
      2);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 0);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 0);

  pair = make_pair(string(kTransactionSecretHeader), string("secret"));
  http_context.request->headers->insert(pair);

  EXPECT_CALL(*mock_transaction_request_router_copy,
              Execute(An<AsyncContext<GetTransactionStatusRequest,
                                      GetTransactionStatusResponse>&>()))
      .WillOnce(
          [&](AsyncContext<GetTransactionStatusRequest,
                           GetTransactionStatusResponse>& transaction_context) {
            Uuid transaction_id;
            EXPECT_SUCCESS(core::common::FromString(
                "3E2A3D09-48ED-A355-D346-AD7DC6CB0909", transaction_id));
            EXPECT_EQ(transaction_context.request->transaction_id,
                      transaction_id);
            EXPECT_EQ(*transaction_context.request->transaction_secret,
                      "secret");
            condition = true;
            return SuccessExecutionResult();
          });

  EXPECT_SUCCESS(front_end_service.GetTransactionStatus(http_context));
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueCoordinator),
      3);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueCoordinator),
      2);
  EXPECT_EQ(
      total_request_metric_instance->GetCounter(kMetricLabelValueOperator), 0);
  EXPECT_EQ(
      client_errors_metric_instance->GetCounter(kMetricLabelValueOperator), 0);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(FrontEndServiceTestWithParam, OnGetTransactionStatusCallback) {
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<JournalServiceInterface> journal_service;
  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer;

  auto mock_transaction_request_router = GetMockTransactionRequestRouter();
  shared_ptr<AsyncExecutorInterface> mock_async_executor =
      make_shared<MockAsyncExecutor>();

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory = GetMockConsumeBudgetCommandFactory();
  shared_ptr<HttpServerInterface> http2_server = make_shared<MockHttp2Server>();
  MockFrontEndServiceWithOverrides front_end_service(
      http2_server, mock_async_executor,
      std::move(mock_transaction_request_router),
      std::move(consume_budget_command_factory), mock_metric_client,
      mock_config_provider);

  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.response = make_shared<HttpResponse>();
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.request =
      make_shared<GetTransactionStatusRequest>();

  auto mock_metric_transaction = make_shared<MockAggregateMetric>();

  vector<size_t> expected_server_error_metrics = {1, 2};

  for (int i = 0; i < results.size(); i++) {
    auto result = results[i];
    atomic<bool> called = false;
    get_transaction_status_context.result = result;

    http_context.callback =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          EXPECT_THAT(http_context.result, ResultIs(result));
          called = true;
        };

    front_end_service.OnGetTransactionStatusCallback(
        mock_metric_transaction, http_context, get_transaction_status_context);

    WaitUntil([&]() { return called.load(); });
    size_t expected_server_error_metric = expected_server_error_metrics[i];
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator),
              expected_server_error_metric);
    EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
              0);
  }

  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->has_failure = false;
  get_transaction_status_context.response->is_expired = true;
  get_transaction_status_context.response->last_execution_timestamp =
      1234519321;
  get_transaction_status_context.response->transaction_execution_phase =
      TransactionExecutionPhase::Abort;
  get_transaction_status_context.result = SuccessExecutionResult();
  atomic<bool> called = false;
  http_context
      .callback = [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
    EXPECT_SUCCESS(http_context.result);
    string response(http_context.response->body.bytes->begin(),
                    http_context.response->body.bytes->end());
    EXPECT_EQ(
        response,
        R"({"has_failures":false,"is_expired":true,"last_execution_timestamp":1234519321,"transaction_execution_phase":"ABORT"})");
    called = true;
  };

  front_end_service.OnGetTransactionStatusCallback(
      mock_metric_transaction, http_context, get_transaction_status_context);
  EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueOperator), 2);
  EXPECT_EQ(mock_metric_transaction->GetCounter(kMetricLabelValueCoordinator),
            0);
  WaitUntil([&]() { return called.load(); });
}

TEST_P(FrontEndServiceTestWithParam, GenerateConsumeBudgetCommands) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket = 1;
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key2");
  consume_budget_metadata_list.back().time_bucket = 10;
  consume_budget_metadata_list.back().token_count = 20;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key3");
  consume_budget_metadata_list.back().time_bucket = 20;
  consume_budget_metadata_list.back().token_count = 40;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands = front_end_service_->GenerateConsumeBudgetCommands(
      consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(generated_commands.size(), consume_budget_metadata_list.size());

  auto consume_budget_metadata_it = consume_budget_metadata_list.begin();

  auto consume_budget_command1 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command1->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command1->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command1->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command1->GetTokenCount());
  EXPECT_EQ(consume_budget_command1->GetBudgetConsumption().request_index, 0);

  consume_budget_metadata_it++;

  auto consume_budget_command2 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[1]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command2->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command2->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command2->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command2->GetTokenCount());
  EXPECT_EQ(consume_budget_command2->GetBudgetConsumption().request_index, 1);

  consume_budget_metadata_it++;

  auto consume_budget_command3 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[2]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command3->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command3->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command3->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command3->GetTokenCount());
  EXPECT_EQ(consume_budget_command3->GetBudgetConsumption().request_index, 2);
}

TEST_P(FrontEndServiceTestWithParam,
       GenerateConsumeBudgetCommandsBatchedPerDaySameKey) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket = (nanoseconds(0)).count();
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(25)).count();
  consume_budget_metadata_list.back().token_count = 20;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(26)).count();
  consume_budget_metadata_list.back().token_count = 40;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands =
      front_end_service_->GenerateConsumeBudgetCommandsWithBatchesPerDay(
          consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(consume_budget_metadata_list.size(), 3);
  EXPECT_EQ(generated_commands.size(), 2);

  auto consume_budget_metadata_it = consume_budget_metadata_list.begin();

  auto consume_budget_command1 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command1->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command1->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command1->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command1->GetTokenCount());
  EXPECT_EQ(*consume_budget_command1->GetBudgetConsumption().request_index, 0);

  consume_budget_metadata_it++;

  auto consume_budget_command2 =
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(generated_commands[1]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command2->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command2->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command2->GetBudgetConsumptions()[0].time_bucket);
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command2->GetBudgetConsumptions()[0].token_count);
  EXPECT_EQ(*consume_budget_command2->GetBudgetConsumptions()[0].request_index,
            1);

  consume_budget_metadata_it++;

  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command2->GetBudgetConsumptions()[1].time_bucket);
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command2->GetBudgetConsumptions()[1].token_count);
  EXPECT_EQ(*consume_budget_command2->GetBudgetConsumptions()[1].request_index,
            2);
}

TEST_P(FrontEndServiceTestWithParam,
       GenerateConsumeBudgetCommandsBatchedPerDayDifferentKeys) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket = (nanoseconds(0)).count();
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key2");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(25)).count();
  consume_budget_metadata_list.back().token_count = 20;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key2");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(29)).count();
  consume_budget_metadata_list.back().token_count = 40;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands =
      front_end_service_->GenerateConsumeBudgetCommandsWithBatchesPerDay(
          consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(consume_budget_metadata_list.size(), 3);
  EXPECT_EQ(generated_commands.size(), 2);

  auto consume_budget_metadata_it = consume_budget_metadata_list.begin();

  auto consume_budget_command1 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command1->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command1->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command1->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command1->GetTokenCount());
  EXPECT_EQ(*consume_budget_command1->GetBudgetConsumption().request_index, 0);

  consume_budget_metadata_it++;

  auto consume_budget_command2 =
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(generated_commands[1]);
  EXPECT_EQ(consume_budget_command2->GetBudgetConsumptions().size(), 2);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command2->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command2->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command2->GetBudgetConsumptions()[0].time_bucket);
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command2->GetBudgetConsumptions()[0].token_count);
  EXPECT_EQ(*consume_budget_command2->GetBudgetConsumptions()[0].request_index,
            1);

  consume_budget_metadata_it++;

  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command2->GetBudgetConsumptions()[1].time_bucket);
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command2->GetBudgetConsumptions()[1].token_count);
  EXPECT_EQ(*consume_budget_command2->GetBudgetConsumptions()[1].request_index,
            2);
}

TEST_P(FrontEndServiceTestWithParam,
       GenerateConsumeBudgetCommandsBatchedPerDayDifferentDaysSameKey) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket = (nanoseconds(0)).count();
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(25)).count();
  consume_budget_metadata_list.back().token_count = 20;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(67)).count();
  consume_budget_metadata_list.back().token_count = 40;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands =
      front_end_service_->GenerateConsumeBudgetCommandsWithBatchesPerDay(
          consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(consume_budget_metadata_list.size(), 3);
  EXPECT_EQ(generated_commands.size(), 3);

  auto consume_budget_metadata_it = consume_budget_metadata_list.begin();

  auto consume_budget_command1 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command1->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command1->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command1->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command1->GetTokenCount());
  EXPECT_EQ(consume_budget_command1->GetBudgetConsumption().request_index, 0);

  consume_budget_metadata_it++;

  auto consume_budget_command2 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[1]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command2->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command2->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command2->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command2->GetTokenCount());
  EXPECT_EQ(consume_budget_command2->GetBudgetConsumption().request_index, 1);

  consume_budget_metadata_it++;

  auto consume_budget_command3 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[2]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command3->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command3->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command3->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command3->GetTokenCount());
  EXPECT_EQ(consume_budget_command3->GetBudgetConsumption().request_index, 2);
}

TEST_P(FrontEndServiceTestWithParam,
       GenerateConsumeBudgetCommandsBatchedPerDayCommonDay) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket = (nanoseconds(0)).count();
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(25)).count();
  consume_budget_metadata_list.back().token_count = 20;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(67)).count();
  consume_budget_metadata_list.back().token_count = 40;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands =
      front_end_service_->GenerateConsumeBudgetCommandsWithBatchesPerDay(
          consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(consume_budget_metadata_list.size(), 3);
  EXPECT_EQ(generated_commands.size(), 3);

  auto consume_budget_metadata_it = consume_budget_metadata_list.begin();

  auto consume_budget_command1 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command1->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command1->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command1->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command1->GetTokenCount());
  EXPECT_EQ(consume_budget_command1->GetBudgetConsumption().request_index, 0);

  consume_budget_metadata_it++;

  auto consume_budget_command2 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[1]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command2->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command2->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command2->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command2->GetTokenCount());
  EXPECT_EQ(consume_budget_command2->GetBudgetConsumption().request_index, 1);

  consume_budget_metadata_it++;

  auto consume_budget_command3 =
      dynamic_pointer_cast<ConsumeBudgetCommand>(generated_commands[2]);
  EXPECT_EQ(auth_domain + std::string("/") +
                *consume_budget_metadata_it->budget_key_name,
            *consume_budget_command3->GetBudgetKeyName());
  EXPECT_EQ(consume_budget_command3->GetTransactionId(), transaction_id);
  EXPECT_EQ(consume_budget_metadata_it->time_bucket,
            consume_budget_command3->GetTimeBucket());
  EXPECT_EQ(consume_budget_metadata_it->token_count,
            consume_budget_command3->GetTokenCount());
  EXPECT_EQ(consume_budget_command3->GetBudgetConsumption().request_index, 2);
}

TEST_F(
    FrontEndServiceTest,
    GenerateConsumeBudgetCommandsBatchedPerDayOrderedBudgetsWithinTimeGroup) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(1)).count();
  consume_budget_metadata_list.back().token_count = 1;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(22)).count();
  consume_budget_metadata_list.back().token_count = 3;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(3)).count();
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(23)).count();
  consume_budget_metadata_list.back().token_count = 4;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands =
      front_end_service_->GenerateConsumeBudgetCommandsWithBatchesPerDay(
          consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(consume_budget_metadata_list.size(), 4);
  EXPECT_EQ(generated_commands.size(), 1);

  auto batch_consume_budget_command =
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/key1"),
            *batch_consume_budget_command->GetBudgetKeyName());
  EXPECT_EQ(batch_consume_budget_command->GetTransactionId(), transaction_id);
  EXPECT_EQ(batch_consume_budget_command->GetBudgetConsumptions().size(), 4);
  auto prev_budget = ConsumeBudgetCommandRequestInfo(0, 0);
  for (auto& budget : batch_consume_budget_command->GetBudgetConsumptions()) {
    EXPECT_TRUE(budget.time_bucket > prev_budget.time_bucket);
    EXPECT_TRUE(budget.token_count > prev_budget.token_count);
    EXPECT_EQ(budget.token_count, prev_budget.token_count + 1);
    prev_budget = budget;
  }
}

TEST_P(FrontEndServiceTestWithParam,
       GenerateConsumeBudgetCommandsBatchedPerDayMultipleBatches) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(22)).count();
  consume_budget_metadata_list.back().token_count = 1;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(1)).count();
  consume_budget_metadata_list.back().token_count = 3;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(25)).count();
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(34)).count();
  consume_budget_metadata_list.back().token_count = 4;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(26)).count();
  consume_budget_metadata_list.back().token_count = 4;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands =
      front_end_service_->GenerateConsumeBudgetCommandsWithBatchesPerDay(
          consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(consume_budget_metadata_list.size(), 5);
  EXPECT_EQ(generated_commands.size(), 2);

  auto batch_consume_budget_command1 =
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/key1"),
            *batch_consume_budget_command1->GetBudgetKeyName());
  EXPECT_EQ(batch_consume_budget_command1->GetTransactionId(), transaction_id);
  EXPECT_EQ(batch_consume_budget_command1->GetBudgetConsumptions().size(), 2);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[0].time_bucket,
      (nanoseconds(0) + hours(1)).count());
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[0].token_count, 3);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[0].request_index,
      1);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[1].time_bucket,
      (nanoseconds(0) + hours(22)).count());
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[1].token_count, 1);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[1].request_index,
      0);

  auto batch_consume_budget_command2 =
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(generated_commands[1]);
  EXPECT_EQ(auth_domain + std::string("/key1"),
            *batch_consume_budget_command2->GetBudgetKeyName());
  EXPECT_EQ(batch_consume_budget_command2->GetTransactionId(), transaction_id);
  EXPECT_EQ(batch_consume_budget_command2->GetBudgetConsumptions().size(), 3);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[0].time_bucket,
      (nanoseconds(0) + hours(25)).count());
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[0].token_count, 2);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[0].request_index,
      2);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[1].time_bucket,
      (nanoseconds(0) + hours(26)).count());
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[1].token_count, 4);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[1].request_index,
      4);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[2].time_bucket,
      (nanoseconds(0) + hours(34)).count());
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[2].token_count, 4);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[2].request_index,
      3);
}

TEST_P(FrontEndServiceTestWithParam,
       GenerateConsumeBudgetCommandsBatchedPerDayMultipleBatchesDiffKeys) {
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(22)).count();
  consume_budget_metadata_list.back().token_count = 1;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key1");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(1)).count();
  consume_budget_metadata_list.back().token_count = 3;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key2");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(25)).count();
  consume_budget_metadata_list.back().token_count = 2;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key2");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(34)).count();
  consume_budget_metadata_list.back().token_count = 4;

  consume_budget_metadata_list.emplace_back();
  consume_budget_metadata_list.back().budget_key_name =
      make_shared<string>("key2");
  consume_budget_metadata_list.back().time_bucket =
      (nanoseconds(0) + hours(26)).count();
  consume_budget_metadata_list.back().token_count = 4;

  auto transaction_id = core::common::Uuid({1, 1});
  auto auth_domain = "origin";

  auto generated_commands =
      front_end_service_->GenerateConsumeBudgetCommandsWithBatchesPerDay(
          consume_budget_metadata_list, auth_domain, transaction_id);
  EXPECT_EQ(consume_budget_metadata_list.size(), 5);
  EXPECT_EQ(generated_commands.size(), 2);

  auto batch_consume_budget_command1 =
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(generated_commands[0]);
  EXPECT_EQ(auth_domain + std::string("/key1"),
            *batch_consume_budget_command1->GetBudgetKeyName());
  EXPECT_EQ(batch_consume_budget_command1->GetTransactionId(), transaction_id);
  EXPECT_EQ(batch_consume_budget_command1->GetBudgetConsumptions().size(), 2);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[0].time_bucket,
      (nanoseconds(0) + hours(1)).count());
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[0].token_count, 3);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[0].request_index,
      1);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[1].time_bucket,
      (nanoseconds(0) + hours(22)).count());
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[1].token_count, 1);
  EXPECT_EQ(
      batch_consume_budget_command1->GetBudgetConsumptions()[1].request_index,
      0);

  auto batch_consume_budget_command2 =
      dynamic_pointer_cast<BatchConsumeBudgetCommand>(generated_commands[1]);
  EXPECT_EQ(auth_domain + std::string("/key2"),
            *batch_consume_budget_command2->GetBudgetKeyName());
  EXPECT_EQ(batch_consume_budget_command2->GetTransactionId(), transaction_id);
  EXPECT_EQ(batch_consume_budget_command2->GetBudgetConsumptions().size(), 3);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[0].time_bucket,
      (nanoseconds(0) + hours(25)).count());
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[0].token_count, 2);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[0].request_index,
      2);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[1].time_bucket,
      (nanoseconds(0) + hours(26)).count());
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[1].token_count, 4);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[1].request_index,
      4);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[2].time_bucket,
      (nanoseconds(0) + hours(34)).count());
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[2].token_count, 4);
  EXPECT_EQ(
      batch_consume_budget_command2->GetBudgetConsumptions()[2].request_index,
      3);
}
}  // namespace google::scp::pbs::test
