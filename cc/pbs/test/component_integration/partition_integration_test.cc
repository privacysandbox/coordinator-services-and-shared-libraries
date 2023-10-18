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

#include <map>
#include <memory>

#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_forwarder/src/http2_forwarder.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/configuration_keys.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/http2_helper/test_http2_server.h"
#include "core/test/utils/logging_utils.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "pbs/front_end_service/src/front_end_service.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/pbs_partition_interface.h"
#include "pbs/partition/src/pbs_partition.h"
#include "pbs/partition_manager/src/pbs_partition_manager.h"
#include "pbs/partition_namespace/src/pbs_partition_namespace.h"
#include "pbs/partition_request_router/src/http_request_route_resolver_for_partition.h"
#include "pbs/partition_request_router/src/transaction_request_router_for_partition.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
#include "pbs/transactions/src/consume_budget_command_factory.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::AuthorizationProxyRequest;
using google::scp::core::AuthorizationProxyResponse;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::Http2Forwarder;
using google::scp::core::Http2Server;
using google::scp::core::Http2ServerOptions;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpClientOptions;
using google::scp::core::HttpRequestRouteResolverInterface;
using google::scp::core::HttpRequestRouterInterface;
using google::scp::core::HttpServerInterface;
using google::scp::core::LoggerInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::PartitionId;
using google::scp::core::PartitionNamespaceInterface;
using google::scp::core::PassThruAuthorizationProxyAsync;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::RetryStrategy;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestHttp2Server;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::PBSPartition;
using google::scp::pbs::PBSPartitionManager;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::seconds;
using std::filesystem::create_directory;
using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::remove_all;
using ::testing::_;
using ::testing::An;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;

static constexpr char kDefaultBucketName[] = "bucket";

namespace google::scp::pbs::test {
class PartitionIntegrationTest : public testing::Test {
 protected:
  void PrepareLogDirectories() {
    if (!exists(kDefaultBucketName)) {
      create_directory(kDefaultBucketName);
    }
  }

  void RemoveLogDirectories() {
    // Remove log folder
    remove_all(kDefaultBucketName);
  }

  PartitionIntegrationTest() {
    // Enable this for SCP Logs.
    //    TestLoggingUtils::EnableLogOutputToConsole();

    metric_client_ = make_shared<MockMetricClient>();
    auto mock_config_provider = make_shared<MockConfigProvider>();
    config_provider_ = mock_config_provider;
    async_executor_ = make_shared<AsyncExecutor>(5 /* thread pool size */,
                                                 100000 /* queue size */,
                                                 true /* drop_tasks_on_stop */);
    HttpClientOptions client_options(
        RetryStrategyOptions(RetryStrategyType::Linear, 100 /* delay in ms */,
                             5 /* num retries */),
        20 /* max connections per host */, 5 /* read timeout in sec */);
    http2_client_ = make_shared<HttpClient>(async_executor_, client_options);

    // Set up fakes for Blob and Table.
    LocalDependencyFactory local_dependency_factory(config_provider_);
    blob_store_provider_ = local_dependency_factory.ConstructBlobStorageClient(
        async_executor_, async_executor_);
    nosql_database_provider_ =
        local_dependency_factory.ConstructNoSQLDatabaseClient(async_executor_,
                                                              async_executor_);

    // Initialize partition dependencies.
    partition_dependencies_.async_executor = async_executor_;
    partition_dependencies_.blob_store_provider = blob_store_provider_;
    partition_dependencies_.blob_store_provider_for_checkpoints =
        blob_store_provider_;
    partition_dependencies_.config_provider = config_provider_;
    partition_dependencies_.metric_client = metric_client_;
    partition_dependencies_.nosql_database_provider_for_background_operations =
        nosql_database_provider_;
    partition_dependencies_.nosql_database_provider_for_live_traffic =
        nosql_database_provider_;
    partition_dependencies_.remote_transaction_manager = nullptr;

    shared_ptr<TokenProviderCacheInterface> dummy_token_provider_cache =
        make_shared<DummyTokenProviderCache>();
    pbs_client_ = make_shared<PrivacyBudgetServiceClient>(
        pbs_client_reporting_origin_,
        "http://" + pbs_endpoint_host_ + ":" + pbs_port_, http2_client_,
        dummy_token_provider_cache);
    request_router_ = make_shared<Http2Forwarder>(http2_client_);
    partition_manager_ = make_shared<PBSPartitionManager>(
        partition_dependencies_, partition_manager_transaction_capacity_);
    partition_namespace_ = make_shared<PBSPartitionNamespace>(partitions_set_);
    request_route_resolver_ = make_shared<HttpRequestRouteResolverForPartition>(
        partition_namespace_, partition_manager_, config_provider_);
    // Authorization is not tested for the purposes of this test.
    shared_ptr<AuthorizationProxyInterface> authorization_proxy =
        make_shared<PassThruAuthorizationProxyAsync>(async_executor_);
    http_server_ = make_shared<Http2Server>(
        pbs_endpoint_host_, pbs_port_, 10 /* http server thread pool size */,
        async_executor_, authorization_proxy, request_router_,
        request_route_resolver_, metric_client_, config_provider_,
        Http2ServerOptions());
    auto transaction_request_router =
        make_unique<TransactionRequestRouterForPartition>(partition_namespace_,
                                                          partition_manager_);
    // Budget key provider is unsed.
    auto consume_budget_command_factory =
        make_unique<ConsumeBudgetCommandFactory>(
            async_executor_, nullptr /* budget key provider */);
    front_end_service_ = make_shared<FrontEndService>(
        http_server_, async_executor_, move(transaction_request_router),
        move(consume_budget_command_factory), metric_client_, config_provider_);

    mock_config_provider->Set(kRemotePrivacyBudgetServiceClaimedIdentity,
                              pbs_remote_coordinator_claimed_identity_);
    mock_config_provider->SetBool(core::kHTTPServerRequestRoutingEnabled, true);
    mock_config_provider->Set(kBudgetKeyTableName, "budget");
    mock_config_provider->Set(kJournalServiceBucketName, kDefaultBucketName);

    // Prepare local log folders for Partitions
    PrepareLogDirectories();

    // Init
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(metric_client_->Init());
    EXPECT_SUCCESS(config_provider_->Init());
    EXPECT_SUCCESS(http_server_->Init());
    EXPECT_SUCCESS(http2_client_->Init());
    EXPECT_SUCCESS(pbs_client_->Init());
    EXPECT_SUCCESS(request_router_->Init());
    EXPECT_SUCCESS(request_route_resolver_->Init());
    EXPECT_SUCCESS(front_end_service_->Init());
    EXPECT_SUCCESS(partition_manager_->Init());

    // Run
    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(metric_client_->Run());
    EXPECT_SUCCESS(config_provider_->Run());
    EXPECT_SUCCESS(http_server_->Run());
    EXPECT_SUCCESS(http2_client_->Run());
    EXPECT_SUCCESS(pbs_client_->Run());
    EXPECT_SUCCESS(request_router_->Run());
    EXPECT_SUCCESS(front_end_service_->Run());
    EXPECT_SUCCESS(partition_manager_->Run());
  }

  void TearDown() override {
    // Stop
    EXPECT_SUCCESS(partition_manager_->Stop());
    EXPECT_SUCCESS(front_end_service_->Stop());
    EXPECT_SUCCESS(request_router_->Stop());
    EXPECT_SUCCESS(http2_client_->Stop());
    EXPECT_SUCCESS(pbs_client_->Stop());
    EXPECT_SUCCESS(metric_client_->Stop());
    EXPECT_SUCCESS(config_provider_->Stop());
    EXPECT_SUCCESS(http_server_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());

    RemoveLogDirectories();
  }

  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
  CreateBudgetKeyConsumptionRequestContext() {
    static atomic<size_t> transaction_id_high_sequencer = 0;
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>
        context;
    context.request = make_shared<ConsumeBudgetTransactionRequest>();
    context.request->budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
    context.request->transaction_id = {1, transaction_id_high_sequencer++};
    context.request->transaction_secret =
        make_shared<string>(pbs_client_secret_);
    return context;
  }

  void GenerateBudgetKeyConsumptions(ConsumeBudgetTransactionRequest& request,
                                     size_t number_of_keys) {
    // Generate two consumptions per key.
    for (int i = 0; i < number_of_keys; i++) {
      request.budget_keys->push_back({make_shared<string>("key" + to_string(i)),
                                      1 /* token */, 1 /* time bucket */});
      request.budget_keys->push_back({make_shared<string>("key" + to_string(i)),
                                      1 /* token */,
                                      1576135250000000000 /* time bucket */});
    }
  }

  // Mocks (reals uneeded for the purposes of the test)
  shared_ptr<core::ConfigProviderInterface> config_provider_;
  shared_ptr<cpio::MetricClientInterface> metric_client_;

  // Fakes
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider_;
  shared_ptr<BlobStorageProviderInterface> blob_store_provider_;

  // Reals
  shared_ptr<PBSPartitionManagerInterface> partition_manager_;
  shared_ptr<PartitionNamespaceInterface> partition_namespace_;
  shared_ptr<HttpRequestRouterInterface> request_router_;
  shared_ptr<HttpRequestRouteResolverInterface> request_route_resolver_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<HttpServerInterface> http_server_;
  shared_ptr<FrontEndService> front_end_service_;
  shared_ptr<HttpClientInterface> http2_client_;
  shared_ptr<PrivacyBudgetServiceClient> pbs_client_;

  string pbs_endpoint_host_ = "localhost";
  string pbs_port_ = "8010";  // TODO: Pick this randomly.
  string pbs_remote_coordinator_claimed_identity_ = "remote-coordinator.com";
  string pbs_client_secret_ = "secret";
  string pbs_client_reporting_origin_ = "foo.com";

  PartitionId partition_id_1_ = PartitionId{1, 2};
  PartitionId partition_id_2_ = PartitionId{1, 3};
  PartitionId partition_id_3_ = PartitionId{1, 4};
  vector<PartitionId> partitions_set_ = {partition_id_1_, partition_id_2_,
                                         partition_id_3_};
  size_t partition_manager_transaction_capacity_ = 10000;

  PBSPartition::Dependencies partition_dependencies_;
};

TEST_F(PartitionIntegrationTest,
       PhaseRequestToLocalPartitionResultsInRetryWhenNonePartitionsLoaded) {
  // No partitions loaded.
  // Initiate Begin.
  atomic<bool> callback_received = false;
  auto request_context = CreateBudgetKeyConsumptionRequestContext();
  GenerateBudgetKeyConsumptions(*request_context.request, 100);
  request_context.callback = [&callback_received](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    callback_received = true;
  };
  EXPECT_SUCCESS(
      pbs_client_->InitiateConsumeBudgetTransaction(request_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });
}

TEST_F(PartitionIntegrationTest,
       RequestToLocalPartitionResultsInRetryWhenThePartitionIsNotLoaded) {
  // 1 out of 3 not loaded.
  // Load partitions
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_1_, core::PartitionType::Local, ""));
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_2_, core::PartitionType::Local, ""));
  // Initiate Begin.
  atomic<bool> callback_received = false;
  auto request_context = CreateBudgetKeyConsumptionRequestContext();
  GenerateBudgetKeyConsumptions(*request_context.request, 100);
  request_context.callback = [&callback_received](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    callback_received = true;
  };

  EXPECT_SUCCESS(
      pbs_client_->InitiateConsumeBudgetTransaction(request_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });
}

TEST_F(PartitionIntegrationTest,
       MultipleRequestsToLocalPartitionResultsInSuccessWhenPartitionIsLoaded) {
  // Load partitions
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_1_, core::PartitionType::Local, ""));
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_2_, core::PartitionType::Local, ""));
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_3_, core::PartitionType::Local, ""));

  // Initiate Begin.
  atomic<bool> callback_received = false;
  auto transaction_request_context = CreateBudgetKeyConsumptionRequestContext();
  GenerateBudgetKeyConsumptions(*transaction_request_context.request, 100);
  transaction_request_context.callback =
      [&callback_received, &transaction_request_context](auto& context) {
        // Copy the response so that it will be used later.
        transaction_request_context.response = context.response;
        EXPECT_SUCCESS(context.result);
        callback_received = true;
      };

  EXPECT_SUCCESS(pbs_client_->InitiateConsumeBudgetTransaction(
      transaction_request_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });

  // Initiate Prepare.
  callback_received = false;
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      prepare_context(make_shared<TransactionPhaseRequest>(),
                      [&callback_received, &prepare_context](auto& context) {
                        prepare_context.response = context.response;
                        EXPECT_SUCCESS(context.result);
                        callback_received = true;
                      });
  prepare_context.request->transaction_id =
      transaction_request_context.request->transaction_id;
  prepare_context.request->last_execution_timestamp =
      transaction_request_context.response->last_execution_timestamp;
  prepare_context.request->transaction_secret =
      make_shared<string>(pbs_client_secret_);
  prepare_context.request->transaction_origin =
      make_shared<string>(pbs_client_reporting_origin_);
  prepare_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Prepare;

  EXPECT_SUCCESS(pbs_client_->ExecuteTransactionPhase(prepare_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });

  // Initiate Commit.
  callback_received = false;
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      commit_context(make_shared<TransactionPhaseRequest>(),
                     [&callback_received, &commit_context](auto& context) {
                       commit_context.response = context.response;
                       EXPECT_SUCCESS(context.result);
                       callback_received = true;
                     });
  commit_context.request->transaction_id =
      transaction_request_context.request->transaction_id;
  commit_context.request->last_execution_timestamp =
      prepare_context.response->last_execution_timestamp;
  commit_context.request->transaction_secret =
      make_shared<string>(pbs_client_secret_);
  commit_context.request->transaction_origin =
      make_shared<string>(pbs_client_reporting_origin_);
  commit_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Commit;

  EXPECT_SUCCESS(pbs_client_->ExecuteTransactionPhase(commit_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });

  // Initiate Commit Notify.
  callback_received = false;
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      notify_context(make_shared<TransactionPhaseRequest>(),
                     [&callback_received, &notify_context](auto& context) {
                       notify_context.response = context.response;
                       EXPECT_SUCCESS(context.result);
                       callback_received = true;
                     });
  notify_context.request->transaction_id =
      transaction_request_context.request->transaction_id;
  notify_context.request->last_execution_timestamp =
      commit_context.response->last_execution_timestamp;
  notify_context.request->transaction_secret =
      make_shared<string>(pbs_client_secret_);
  notify_context.request->transaction_origin =
      make_shared<string>(pbs_client_reporting_origin_);
  notify_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Notify;

  EXPECT_SUCCESS(pbs_client_->ExecuteTransactionPhase(notify_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });
}

TEST_F(PartitionIntegrationTest,
       PhaseRequestsToPartitionForwardedWhenPartitionIsLoadedButNotLocal) {
  // Load partitions
  partition_manager_->LoadPartition(core::PartitionMetadata(
      partition_id_1_, core::PartitionType::Remote, "https://www.google.com"));
  partition_manager_->LoadPartition(core::PartitionMetadata(
      partition_id_2_, core::PartitionType::Remote, "https://www.google.com"));
  partition_manager_->LoadPartition(core::PartitionMetadata(
      partition_id_3_, core::PartitionType::Remote, "https://www.google.com"));

  // Initiate Begin.
  atomic<bool> callback_received = false;
  auto request_context = CreateBudgetKeyConsumptionRequestContext();
  GenerateBudgetKeyConsumptions(*request_context.request, 100);
  request_context.callback = [&callback_received](auto& result_context) {
    // "https:://www.google.com/ returns a NOT FOUND error.
    EXPECT_THAT(result_context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND)));
    callback_received = true;
  };

  EXPECT_SUCCESS(
      pbs_client_->InitiateConsumeBudgetTransaction(request_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });
}

TEST_F(PartitionIntegrationTest, BunchOfRequestsToRemotePartitionCanBeHandled) {
  // Load partitions
  partition_manager_->LoadPartition(core::PartitionMetadata(
      partition_id_1_, core::PartitionType::Remote, "https://www.google.com"));
  partition_manager_->LoadPartition(core::PartitionMetadata(
      partition_id_2_, core::PartitionType::Remote, "https://www.google.com"));
  partition_manager_->LoadPartition(core::PartitionMetadata(
      partition_id_3_, core::PartitionType::Remote, "https://www.google.com"));
  // Send a bunch of requests
  uint64_t thread_count = 10;
  uint64_t request_per_thread = 300;
  vector<thread> threads;
  for (int i = 0; i < thread_count; i++) {
    threads.emplace_back([this, request_per_thread]() {
      atomic<size_t> callback_received(0);
      for (int i = 0; i < request_per_thread; i++) {
        // Initiate Begin.
        auto request_context = CreateBudgetKeyConsumptionRequestContext();
        GenerateBudgetKeyConsumptions(*request_context.request, 10);
        request_context.callback = [&callback_received](auto& result_context) {
          callback_received++;
        };
        EXPECT_SUCCESS(
            pbs_client_->InitiateConsumeBudgetTransaction(request_context));
      }
      WaitUntil(
          [&callback_received, request_per_thread]() {
            return callback_received.load() >= request_per_thread;
          },
          seconds(40));
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(PartitionIntegrationTest, BunchOfRequestsToLocalPartitionCanBeHandled) {
  // Load partitions
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_1_, core::PartitionType::Local, ""));
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_2_, core::PartitionType::Local, ""));
  partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_3_, core::PartitionType::Local, ""));
  // Send a bunch of requests
  uint64_t thread_count = 10;
  uint64_t request_per_thread = 200;
  vector<thread> threads;
  for (int i = 0; i < thread_count; i++) {
    threads.emplace_back([this, request_per_thread]() {
      atomic<size_t> callback_received(0);
      for (int i = 0; i < request_per_thread; i++) {
        // Initiate Begin.
        auto request_context = CreateBudgetKeyConsumptionRequestContext();
        GenerateBudgetKeyConsumptions(*request_context.request, 5);
        request_context.callback = [&callback_received](auto& result_context) {
          // If there is a retry, PreCondition failed might happen as the
          // transaction request has already been processed.
          // See. SC_TRANSACTION_MANAGER_TRANSACTION_ALREADY_EXISTS
          EXPECT_THAT(
              result_context.result,
              AnyOf(IsSuccessful(),
                    ResultIs(FailureExecutionResult(
                        core::errors::
                            SC_HTTP2_CLIENT_HTTP_STATUS_PRECONDITION_FAILED))));
          callback_received++;
        };
        EXPECT_SUCCESS(
            pbs_client_->InitiateConsumeBudgetTransaction(request_context));
      }
      WaitUntil(
          [&callback_received, request_per_thread]() {
            return callback_received.load() >= request_per_thread;
          },
          seconds(40));
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(PartitionIntegrationTest,
       BunchOfRequestsToPartitionNotLoadedCanBeHandled) {
  // Send a bunch of requests
  uint64_t thread_count = 10;
  uint64_t request_per_thread = 300;
  vector<thread> threads;
  for (int i = 0; i < thread_count; i++) {
    threads.emplace_back([this, request_per_thread]() {
      atomic<size_t> callback_received(0);
      for (int i = 0; i < request_per_thread; i++) {
        // Initiate Begin.
        auto request_context = CreateBudgetKeyConsumptionRequestContext();
        GenerateBudgetKeyConsumptions(*request_context.request, 10);
        request_context.callback = [&callback_received](auto& result_context) {
          callback_received++;
        };
        EXPECT_SUCCESS(
            pbs_client_->InitiateConsumeBudgetTransaction(request_context));
      }
      WaitUntil(
          [&callback_received, request_per_thread]() {
            return callback_received.load() >= request_per_thread;
          },
          seconds(40));
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
}  // namespace google::scp::pbs::test
