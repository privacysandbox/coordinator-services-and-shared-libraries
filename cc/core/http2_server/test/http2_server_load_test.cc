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

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/http2_client/src/http2_client.h"
#include "cc/core/http2_server/src/http2_server.h"
#include "cc/core/interface/errors.h"
#include "cc/core/test/utils/conditional_wait.h"
#include "cc/core/test/utils/logging_utils.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {
namespace {
using ::testing::_;
using ::testing::An;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Eq;

class HttpServerLoadTest : public testing::Test {
 protected:
  HttpServerLoadTest() {
    auto mock_config_provider = std::make_shared<MockConfigProvider>();
    config_provider_ = mock_config_provider;
    async_executor_for_server_ = std::make_shared<AsyncExecutor>(
        20 /* thread pool size */, 100000 /* queue size */,
        true /* drop_tasks_on_stop */);
    async_executor_for_client_ = std::make_shared<AsyncExecutor>(
        20 /* thread pool size */, 100000 /* queue size */,
        true /* drop_tasks_on_stop */);
    HttpClientOptions client_options(
        RetryStrategyOptions(RetryStrategyType::Linear, 100 /* delay in ms */,
                             5 /* num retries */),
        1 /* max connections per host */, 5 /* read timeout in sec */);
    http2_client_ = std::make_shared<HttpClient>(async_executor_for_client_,
                                                 client_options);

    // Authorization is not tested for the purposes of this test.
    std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
        std::make_shared<PassThruAuthorizationProxy>();

    http_server_ = std::make_shared<Http2Server>(
        host_, port_, 10 /* http server thread pool size */,
        async_executor_for_server_, authorization_proxy,
        /*aws_authorization_proxy=*/nullptr, config_provider_);

    std::string path = "/v1/test";
    HttpHandler handler =
        [this](AsyncContext<HttpRequest, HttpResponse>& context) {
          total_requests_received_on_server++;
          context.response = std::make_shared<HttpResponse>();
          context.response->body =
              BytesBuffer(std::to_string(total_requests_received_on_server));
          context.response->code = HttpStatusCode(200);
          context.result = SuccessExecutionResult();
          context.Finish();
          return SuccessExecutionResult();
        };
    EXPECT_SUCCESS(
        http_server_->RegisterResourceHandler(HttpMethod::POST, path, handler));

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

  std::string host_ = "localhost";
  std::string port_ = "8099";  // TODO: Pick this randomly.
  std::shared_ptr<ConfigProviderInterface> config_provider_;
  std::shared_ptr<AsyncExecutorInterface> async_executor_for_server_;
  std::shared_ptr<AsyncExecutorInterface> async_executor_for_client_;
  std::shared_ptr<HttpServerInterface> http_server_;
  std::shared_ptr<HttpClientInterface> http2_client_;
  std::atomic<size_t> total_requests_received_on_server = 0;
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

  std::atomic<bool> is_qps_thread_stopped = false;
  std::thread qps_thread([this, &is_qps_thread_stopped]() {
    auto req_prev = total_requests_received_on_server.load();
    while (!is_qps_thread_stopped) {
      auto req = total_requests_received_on_server.load();
      std::cout << "QPS: " << req - req_prev << std::endl;
      req_prev = req;
      std::this_thread::sleep_for(std::chrono::seconds(1));
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
    std::atomic<size_t> client_requests_completed_in_current_round = 0;

    // Initialize a bunch of clients.
    std::vector<std::shared_ptr<HttpClientInterface>> http2_clients;
    for (int j = 0; j < num_clients; j++) {
      auto http2_client = std::make_shared<HttpClient>(
          async_executor_for_client_, client_options);
      EXPECT_SUCCESS(http2_client->Init());
      EXPECT_SUCCESS(http2_client->Run());
      http2_clients.push_back(http2_client);
    }

    // Send requests on each of the http clients.
    std::cout << "Round " << i + 1 << ": "
              << "Initialized clients. Sending requests..." << std::endl;
    for (auto& http2_client : http2_clients) {
      auto request = std::make_shared<HttpRequest>();
      request->method = HttpMethod::POST;
      request->path = std::make_shared<std::string>("http://" + host_ + ":" +
                                                    port_ + "/v1/test");
      AsyncContext<HttpRequest, HttpResponse> request_context(
          std::move(request),
          [&](AsyncContext<HttpRequest, HttpResponse>& result_context) {
            client_requests_completed_in_current_round++;
          });
      EXPECT_SUCCESS(http2_client->PerformRequest(request_context));
    }

    while (client_requests_completed_in_current_round < num_clients) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Round " << i + 1 << ": "
              << "client_requests_completed_in_current_round: "
              << client_requests_completed_in_current_round
              << " total_requests_received_on_server: "
              << total_requests_received_on_server << std::endl;

    // Send another round of multiple requests on the same set of clients.
    for (auto& http2_client : http2_clients) {
      auto request = std::make_shared<HttpRequest>();
      request->method = HttpMethod::POST;
      request->path = std::make_shared<std::string>("http://" + host_ + ":" +
                                                    port_ + "/v1/test");
      AsyncContext<HttpRequest, HttpResponse> request_context(
          std::move(request),
          [&](AsyncContext<HttpRequest, HttpResponse>& result_context) {
            client_requests_completed_in_current_round++;
          });
      for (int i = 0; i < requests_per_client; i++) {
        EXPECT_SUCCESS(http2_client->PerformRequest(request_context));
      }
    }

    while (client_requests_completed_in_current_round <
           (num_clients * requests_per_client)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Round " << i + 1 << ": "
              << "client_requests_completed_in_current_round: "
              << client_requests_completed_in_current_round
              << " total_requests_received_on_server: "
              << total_requests_received_on_server << std::endl;

    std::cout << "Stopping clients" << std::endl;

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

}  // namespace
}  // namespace privacy_sandbox::pbs_common
