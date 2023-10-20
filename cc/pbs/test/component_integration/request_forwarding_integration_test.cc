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
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_forwarder/src/http2_forwarder.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/configuration_keys.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/http2_helper/test_http2_server.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "pbs/front_end_service/src/front_end_service.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/pbs_partition_interface.h"
#include "pbs/partition/mock/pbs_partition_mock.h"
#include "pbs/partition_namespace/src/pbs_partition_namespace.h"
#include "pbs/partition_request_router/src/http_request_route_resolver_for_partition.h"
#include "pbs/partition_request_router/src/transaction_request_router_for_partition.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "pbs/transactions/src/consume_budget_command_factory.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AuthorizationProxyInterface;
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
using google::scp::core::PartitionId;
using google::scp::core::PartitionNamespaceInterface;
using google::scp::core::PassThruAuthorizationProxy;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::test::TestHttp2Server;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::partition::mock::MockPBSPartition;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using ::testing::_;
using ::testing::An;
using ::testing::DoAll;
using ::testing::Return;

namespace google::scp::pbs::test {

class FakePBSPartitionManager : public PBSPartitionManagerInterface {
 public:
  struct PartitionEntry {
    std::shared_ptr<core::Uri> partition_host_uri;
    std::shared_ptr<PBSPartitionInterface> pbs_partition;
  };

  ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  ExecutionResult LoadPartition(
      const core::PartitionMetadata& partition_metadata) noexcept override {
    partitions_[partition_metadata.Id()] = PartitionEntry{
        std::make_shared<string>(partition_metadata.partition_address_uri),
        make_shared<MockPBSPartition>()};
    return SuccessExecutionResult();
  }

  ExecutionResult UnloadPartition(
      const core::PartitionMetadata& partition_metadata) noexcept override {
    partitions_.erase(partition_metadata.Id());
    return SuccessExecutionResult();
  }

  ExecutionResult RefreshPartitionAddress(
      const core::PartitionMetadata& partition_metadata) noexcept override {
    partitions_[partition_metadata.Id()].partition_host_uri =
        make_shared<string>(partition_metadata.partition_address_uri);
    return SuccessExecutionResult();
  }

  core::ExecutionResultOr<shared_ptr<core::PartitionAddressUri>>
  GetPartitionAddress(const core::PartitionId& partition_id) noexcept override {
    return partitions_[partition_id].partition_host_uri;
  }

  core::ExecutionResultOr<core::PartitionType> GetPartitionType(
      const core::PartitionId& partition_id) noexcept override {
    // Unused.
    return FailureExecutionResult(1234);
  }

  core::ExecutionResultOr<shared_ptr<core::PartitionInterface>> GetPartition(
      const core::PartitionId& partition_id) noexcept override {
    return partitions_[partition_id].pbs_partition;
  }

  core::ExecutionResultOr<shared_ptr<PBSPartitionInterface>> GetPBSPartition(
      const core::PartitionId& partition_id) noexcept override {
    return partitions_[partition_id].pbs_partition;
  }

 private:
  map<core::PartitionId, PartitionEntry> partitions_;
};

class RequestForwardingIntegrationTest : public testing::Test {
 protected:
  RequestForwardingIntegrationTest() {
    metric_client_ = make_shared<MockMetricClient>();
    auto mock_config_provider = make_shared<MockConfigProvider>();
    config_provider_ = mock_config_provider;
    async_executor_ = make_shared<AsyncExecutor>(5 /* thread pool size */,
                                                 100000 /* queue size */,
                                                 true /* drop_tasks_on_stop */);

    http2_client_ = make_shared<HttpClient>(async_executor_);
    shared_ptr<TokenProviderCacheInterface> dummy_token_provider_cache =
        make_shared<DummyTokenProviderCache>();
    pbs_client_ = make_shared<PrivacyBudgetServiceClient>(
        pbs_client_reporting_origin_,
        "http://" + pbs_endpoint_host_ + ":" + pbs_port_, http2_client_,
        dummy_token_provider_cache);
    request_router_ = make_shared<Http2Forwarder>(http2_client_);
    fake_partition_manager_ = make_shared<FakePBSPartitionManager>();
    partition_manager_ = fake_partition_manager_;
    partition_namespace_ = make_shared<PBSPartitionNamespace>(partitions_set_);
    request_route_resolver_ = make_shared<HttpRequestRouteResolverForPartition>(
        partition_namespace_, partition_manager_, config_provider_);
    // Authorization is not tested for the purposes of this test.
    shared_ptr<AuthorizationProxyInterface> authorization_proxy =
        make_shared<PassThruAuthorizationProxy>();
    http_server_ = make_shared<Http2Server>(
        pbs_endpoint_host_, pbs_port_, 5 /* http server thread pool size */,
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

    // Run
    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(metric_client_->Run());
    EXPECT_SUCCESS(config_provider_->Run());
    EXPECT_SUCCESS(http_server_->Run());
    EXPECT_SUCCESS(http2_client_->Run());
    EXPECT_SUCCESS(pbs_client_->Run());
    EXPECT_SUCCESS(request_router_->Run());
    EXPECT_SUCCESS(front_end_service_->Run());
  }

  void TearDown() override {
    // Stop
    EXPECT_SUCCESS(front_end_service_->Stop());
    EXPECT_SUCCESS(request_router_->Stop());
    EXPECT_SUCCESS(http2_client_->Stop());
    EXPECT_SUCCESS(pbs_client_->Stop());
    EXPECT_SUCCESS(metric_client_->Stop());
    EXPECT_SUCCESS(config_provider_->Stop());
    EXPECT_SUCCESS(http_server_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
  }

  // Mocks (reals uneeded for the purposes of the test)
  shared_ptr<core::ConfigProviderInterface> config_provider_;
  shared_ptr<cpio::MetricClientInterface> metric_client_;

  // Fake
  shared_ptr<PBSPartitionManagerInterface> partition_manager_;
  shared_ptr<FakePBSPartitionManager> fake_partition_manager_;

  // Reals
  shared_ptr<PartitionNamespaceInterface> partition_namespace_;
  shared_ptr<HttpRequestRouterInterface> request_router_;
  shared_ptr<HttpRequestRouteResolverInterface> request_route_resolver_;
  shared_ptr<core::AsyncExecutorInterface> async_executor_;
  shared_ptr<core::HttpServerInterface> http_server_;
  shared_ptr<FrontEndService> front_end_service_;
  shared_ptr<HttpClientInterface> http2_client_;
  shared_ptr<PrivacyBudgetServiceClient> pbs_client_;

  string pbs_endpoint_host_ = "localhost";
  string pbs_port_ = "8011";  // TODO: Pick this randomly.
  string pbs_remote_coordinator_claimed_identity_ = "remote-coordinator.com";
  string pbs_client_secret_ = "secret";
  string pbs_client_reporting_origin_ = "foo.com";
  PartitionId partition_id_ = PartitionId{1, 2};
  vector<PartitionId> partitions_set_ = {partition_id_};
};

TEST_F(RequestForwardingIntegrationTest, RequestIsHandledIfPartitionIsLocal) {
  // Initialize partition in partition manager.
  fake_partition_manager_->LoadPartition(
      core::PartitionMetadata(partition_id_, core::PartitionType::Local, ""));
  auto partition = fake_partition_manager_->GetPBSPartition(partition_id_);
  auto mock_partition = std::dynamic_pointer_cast<MockPBSPartition>(*partition);
  EXPECT_CALL(*mock_partition,
              ExecuteRequest(An<AsyncContext<TransactionPhaseRequest,
                                             TransactionPhaseResponse>&>()))
      .WillOnce([](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<TransactionPhaseResponse>();
        context.response->last_execution_timestamp = 1212;
        FinishContext(context.result, context);
        return SuccessExecutionResult();
      });

  atomic<bool> callback_received = false;
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          [&callback_received](auto& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_EQ(context.response->last_execution_timestamp, 1212);
            callback_received = true;
          });
  transaction_phase_context.request->transaction_id =
      core::common::Uuid{123, 456};
  transaction_phase_context.request->transaction_secret =
      make_shared<string>(pbs_client_secret_);
  transaction_phase_context.request->transaction_origin =
      make_shared<string>(pbs_client_reporting_origin_);
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Commit;

  EXPECT_SUCCESS(
      pbs_client_->ExecuteTransactionPhase(transaction_phase_context));
  WaitUntil([&callback_received]() { return callback_received.load(); });
}

TEST_F(RequestForwardingIntegrationTest,
       RequestIsForwardedIfPartitionIsRemote) {
  // Start a local server for handling COMMIT path.
  TestHttp2Server server;
  server.Handle(string(kCommitTransactionPath),
                [&](const nghttp2::asio_http2::server::request& req,
                    const nghttp2::asio_http2::server::response& res) {
                  // Commit executes and returns a execution timestamp.
                  res.write_head(
                      200, {{string(kTransactionLastExecutionTimestampHeader),
                             {std::to_string(1212), true}}});
                  res.end();
                });
  server.Run();

  // Initialize partition in partition manager.
  fake_partition_manager_->LoadPartition(core::PartitionMetadata(
      partition_id_, core::PartitionType::Remote,
      absl::StrCat("http://", server.HostName(), ":", server.PortNumber())));

  atomic<bool> callback_received = false;
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          [&callback_received](auto& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_EQ(context.response->last_execution_timestamp, 1212);
            callback_received = true;
          });
  transaction_phase_context.request->transaction_id =
      core::common::Uuid{123, 456};
  transaction_phase_context.request->transaction_secret =
      make_shared<string>(pbs_client_secret_);
  transaction_phase_context.request->transaction_origin =
      make_shared<string>(pbs_client_reporting_origin_);
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::Commit;

  EXPECT_SUCCESS(
      pbs_client_->ExecuteTransactionPhase(transaction_phase_context));
  WaitUntil([&]() { return callback_received.load(); });
}

}  // namespace google::scp::pbs::test
