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
#include <string>

#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::BytesBuffer;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http2Server;
using google::scp::core::Http2ServerOptions;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpClientOptions;
using google::scp::core::HttpRequest;
using google::scp::core::HttpRequestRouteResolverInterface;
using google::scp::core::HttpRequestRouterInterface;
using google::scp::core::HttpResponse;
using google::scp::core::HttpServerInterface;
using google::scp::core::LoggerInterface;
using google::scp::core::PassThruAuthorizationProxy;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::RetryStrategy;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::errors::HttpStatusCode;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockMetricClient;
using std::atomic;
using std::cout;
using std::endl;
using std::make_shared;
using std::make_unique;
using std::map;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;
using ::testing::_;
using ::testing::An;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;

namespace google::scp::pbs::test {

class HttpServerLoadTest : public testing::Test {
 protected:
  HttpServerLoadTest() {
    metric_client_ = make_shared<MockMetricClient>();
    auto mock_config_provider = make_shared<MockConfigProvider>();
    config_provider_ = mock_config_provider;
    async_executor_for_server_ = make_shared<AsyncExecutor>(
        20 /* thread pool size */, 100000 /* queue size */,
        true /* drop_tasks_on_stop */);
    async_executor_for_client_ = make_shared<AsyncExecutor>(
        20 /* thread pool size */, 100000 /* queue size */,
        true /* drop_tasks_on_stop */);
    HttpClientOptions client_options(
        RetryStrategyOptions(RetryStrategyType::Linear, 100 /* delay in ms */,
                             5 /* num retries */),
        1 /* max connections per host */, 5 /* read timeout in sec */);
    http2_client_ =
        make_shared<HttpClient>(async_executor_for_client_, client_options);

    // Authorization is not tested for the purposes of this test.
    shared_ptr<AuthorizationProxyInterface> authorization_proxy =
        make_shared<PassThruAuthorizationProxy>();

    http_server_ = make_shared<Http2Server>(
        host_, port_, 10 /* http server thread pool size */,
        async_executor_for_server_, authorization_proxy, metric_client_,
        config_provider_);

    string path = "/v1/test";
    core::HttpHandler handler =
        [this](AsyncContext<HttpRequest, HttpResponse>& context) {
          total_requests_received_on_server++;
          context.response = make_shared<HttpResponse>();
          context.response->body =
              BytesBuffer(to_string(total_requests_received_on_server));
          context.response->code = HttpStatusCode(200);
          context.result = SuccessExecutionResult();
          context.Finish();
          return SuccessExecutionResult();
        };
    EXPECT_SUCCESS(http_server_->RegisterResourceHandler(core::HttpMethod::POST,
                                                         path, handler));

    // Init
    EXPECT_SUCCESS(async_executor_for_client_->Init());
    EXPECT_SUCCESS(async_executor_for_server_->Init());
    EXPECT_SUCCESS(http_server_->Init());
    EXPECT_SUCCESS(http2_client_->Init());

    // Run
    EXPECT_SUCCESS(async_executor_for_client_->Run());
    EXPECT_SUCCESS(async_executor_for_server_->Run());
    EXPECT_SUCCESS(http_server_->Run());
    EXPECT_SUCCESS(http2_client_->Run());
  }

  void TearDown() override {
    // Stop
    EXPECT_SUCCESS(http2_client_->Stop());
    EXPECT_SUCCESS(http_server_->Stop());
    EXPECT_SUCCESS(async_executor_for_client_->Stop());
    EXPECT_SUCCESS(async_executor_for_server_->Stop());
  }

  string host_ = "localhost";
  string port_ = "8099";  // TODO: Pick this randomly.
  shared_ptr<core::ConfigProviderInterface> config_provider_;
  shared_ptr<cpio::MetricClientInterface> metric_client_;
  shared_ptr<AsyncExecutorInterface> async_executor_for_server_;
  shared_ptr<AsyncExecutorInterface> async_executor_for_client_;
  shared_ptr<HttpServerInterface> http_server_;
  shared_ptr<HttpClientInterface> http2_client_;
  atomic<size_t> total_requests_received_on_server = 0;
};

TEST_F(HttpServerLoadTest,
       LoadTestWithSeveralClientsDoesNotStallServerOrCrash) {
  // Number of requests per client, number of clients.
  size_t requests_per_client = 5;
  size_t num_clients = 2500;
  // Each round creates a fresh set of clients of 'num_clients' count and eaech
  // client sends requests of 'requests_per_client' count
  size_t num_rounds = 5;
  size_t connections_per_client = 1;
  size_t client_connection_read_timeout_in_seconds = 4;

  atomic<bool> is_qps_thread_stopped = false;
  thread qps_thread([this, &is_qps_thread_stopped]() {
    auto req_prev = total_requests_received_on_server.load();
    while (!is_qps_thread_stopped) {
      auto req = total_requests_received_on_server.load();
      cout << "QPS: " << req - req_prev << endl;
      req_prev = req;
      sleep_for(seconds(1));
    }
  });

  HttpClientOptions client_options(
      RetryStrategyOptions(RetryStrategyType::Linear, 100 /* delay in ms */,
                           5 /* num retries */),
      connections_per_client, client_connection_read_timeout_in_seconds);

  for (int i = 0; i < num_rounds; i++) {
    size_t total_requests_received_on_server_prev =
        total_requests_received_on_server;

    // Reset the counter to process new clients.
    atomic<size_t> client_requests_completed_in_current_round = 0;

    // Initialize a bunch of clients.
    vector<shared_ptr<HttpClientInterface>> http2_clients;
    for (int j = 0; j < num_clients; j++) {
      auto http2_client =
          make_shared<HttpClient>(async_executor_for_client_, client_options);
      EXPECT_SUCCESS(http2_client->Init());
      EXPECT_SUCCESS(http2_client->Run());
      http2_clients.push_back(http2_client);
    }

    // Send requests on each of the http clients.
    cout << "Round " << i + 1 << ": "
         << "Initialized clients. Sending requests..." << endl;
    for (auto& http2_client : http2_clients) {
      auto request = make_shared<HttpRequest>();
      request->method = core::HttpMethod::POST;
      request->path =
          make_shared<string>("http://" + host_ + ":" + port_ + "/v1/test");
      AsyncContext<HttpRequest, HttpResponse> request_context(
          move(request),
          [&](AsyncContext<HttpRequest, HttpResponse>& result_context) {
            client_requests_completed_in_current_round++;
          });
      EXPECT_SUCCESS(http2_client->PerformRequest(request_context));
    }

    while (client_requests_completed_in_current_round < num_clients) {
      sleep_for(milliseconds(100));
    }

    cout << "Round " << i + 1 << ": "
         << "client_requests_completed_in_current_round: "
         << client_requests_completed_in_current_round
         << " total_requests_received_on_server: "
         << total_requests_received_on_server << endl;

    // Send another round of multiple requests on the same set of clients.
    for (auto& http2_client : http2_clients) {
      auto request = make_shared<HttpRequest>();
      request->method = core::HttpMethod::POST;
      request->path =
          make_shared<string>("http://" + host_ + ":" + port_ + "/v1/test");
      AsyncContext<HttpRequest, HttpResponse> request_context(
          move(request),
          [&](AsyncContext<HttpRequest, HttpResponse>& result_context) {
            client_requests_completed_in_current_round++;
          });
      for (int i = 0; i < requests_per_client; i++) {
        EXPECT_SUCCESS(http2_client->PerformRequest(request_context));
      }
    }

    while (client_requests_completed_in_current_round <
           (num_clients * requests_per_client)) {
      sleep_for(milliseconds(100));
    }

    cout << "Round " << i + 1 << ": "
         << "client_requests_completed_in_current_round: "
         << client_requests_completed_in_current_round
         << " total_requests_received_on_server: "
         << total_requests_received_on_server << endl;

    cout << "Stopping clients" << endl;

    for (auto& http2_client : http2_clients) {
      EXPECT_SUCCESS(http2_client->Stop());
    }

    // Check no stall.
    EXPECT_GT(total_requests_received_on_server,
              total_requests_received_on_server_prev);
  }

  is_qps_thread_stopped = true;
  qps_thread.join();
}
}  // namespace google::scp::pbs::test
