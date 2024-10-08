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

#include "core/http2_server/src/http2_server.h"

#include <gtest/gtest.h>

#include <future>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>

#include "absl/random/random.h"
#include "absl/random/uniform_int_distribution.h"
#include "absl/synchronization/blocking_counter.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/mock/mock_authorization_proxy.h"
#include "core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_server/mock/mock_http2_request_with_overrides.h"
#include "core/http2_server/mock/mock_http2_response_with_overrides.h"
#include "core/http2_server/mock/mock_http2_server_with_overrides.h"
#include "core/http2_server/src/error_codes.h"
#include "core/telemetry/mock/in_memory_metric_router.h"
#include "core/telemetry/src/common/metric_utils.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "nlohmann/json.hpp"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

namespace google::scp::core {
namespace {

using ::google::scp::core::AsyncExecutor;
using ::google::scp::core::AuthorizationProxyInterface;
using ::google::scp::core::Http2Server;
using ::google::scp::core::HttpClient;
using ::google::scp::core::async_executor::mock::MockAsyncExecutor;
using ::google::scp::core::authorization_proxy::mock::MockAuthorizationProxy;
using ::google::scp::core::common::Uuid;
using ::google::scp::core::config_provider::mock::MockConfigProvider;
using ::google::scp::core::errors::HttpStatusCode;
using ::google::scp::core::http2_server::mock::MockHttp2ServerWithOverrides;
using ::google::scp::core::http2_server::mock::MockNgHttp2RequestWithOverrides;
using ::google::scp::core::http2_server::mock::MockNgHttp2ResponseWithOverrides;
using ::google::scp::core::test::ResultIs;
using ::google::scp::core::test::WaitUntil;
using ::google::scp::core::utils::Base64Encode;
using ::google::scp::core::utils::PadBase64Encoding;
using ::google::scp::cpio::MockMetricClient;
using ::testing::Return;

class Http2ServerTest : public testing::Test {
 protected:
  void SetUp() override {
    // Generate a self-signed cert
    system("openssl genrsa 2048 > privatekey.pem");
    system(
        "openssl req -new -key privatekey.pem -out csr.pem -config "
        "cc/core/http2_server/test/certs/csr.conf");
    system(
        "openssl x509 -req -days 7305 -in csr.pem -signkey privatekey.pem -out "
        "public.crt");

    mock_config_provider_ = std::make_shared<MockConfigProvider>();
    mock_config_provider_->SetBool(kHttpServerDnsRoutingEnabled, true);
  }

  std::shared_ptr<MockConfigProvider> mock_config_provider_;
};

enum class Http2ServerTestCloud { kAws, kGcp };

std::shared_ptr<MockNgHttp2RequestWithOverrides> CreateMockRequest(
    Http2ServerTestCloud cloud = Http2ServerTestCloud::kGcp) {
  nghttp2::asio_http2::server::request request;
  auto mock_http2_request =
      std::make_shared<MockNgHttp2RequestWithOverrides>(request);
  auto headers = std::make_shared<HttpHeaders>();
  nlohmann::json json_token;
  if (cloud == Http2ServerTestCloud::kAws) {
    json_token["amz_date"] = "hello_date_value";
  }
  std::string token_from_dumped_json = json_token.dump();

  std::string encoded_token;
  Base64Encode(token_from_dumped_json, encoded_token);
  headers->insert({kAuthHeader, encoded_token});
  headers->insert({kClaimedIdentityHeader, "https://origin.site.com"});
  mock_http2_request->headers = headers;
  return mock_http2_request;
}

TEST_F(Http2ServerTest, Run) {
  std::string host_address("localhost");
  std::string port("0");

  std::shared_ptr<AuthorizationProxyInterface> mock_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  auto mock_metric_client = std::make_shared<MockMetricClient>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  Http2Server http_server(host_address, port, 2 /* thread_pool_size */,
                          async_executor, mock_authorization_proxy,
                          mock_aws_authorization_proxy);

  EXPECT_SUCCESS(http_server.Run());
  EXPECT_THAT(http_server.Run(), ResultIs(FailureExecutionResult(
                                     errors::SC_HTTP2_SERVER_ALREADY_RUNNING)));

  EXPECT_SUCCESS(http_server.Stop());
  EXPECT_THAT(http_server.Stop(),
              ResultIs(FailureExecutionResult(
                  errors::SC_HTTP2_SERVER_ALREADY_STOPPED)));
}

TEST_F(Http2ServerTest, RegisterHandlers) {
  std::string host_address("localhost");
  std::string port("0");

  std::shared_ptr<AuthorizationProxyInterface> mock_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  auto mock_metric_client = std::make_shared<MockMetricClient>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  MockHttp2ServerWithOverrides http_server(
      host_address, port, async_executor, mock_authorization_proxy,
      mock_aws_authorization_proxy, mock_metric_client, mock_config_provider_);

  std::string path("/test/path");
  HttpHandler callback = [](AsyncContext<HttpRequest, HttpResponse>&) {
    return SuccessExecutionResult();
  };

  EXPECT_SUCCESS(
      http_server.RegisterResourceHandler(HttpMethod::GET, path, callback));

  EXPECT_THAT(
      http_server.RegisterResourceHandler(HttpMethod::GET, path, callback),
      ResultIs(FailureExecutionResult(
          errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));
}

TEST_F(Http2ServerTest, HandleHttp2Request) {
  std::string host_address("localhost");
  std::string port("0");

  auto mock_authorization_proxy = std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      mock_authorization_proxy;
  EXPECT_CALL(*mock_authorization_proxy, Authorize)
      .WillOnce(Return(SuccessExecutionResult()));
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  auto mock_metric_client = std::make_shared<MockMetricClient>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  MockHttp2ServerWithOverrides http_server(
      host_address, port, async_executor, authorization_proxy,
      mock_aws_authorization_proxy, mock_metric_client, mock_config_provider_);

  HttpHandler callback = [](AsyncContext<HttpRequest, HttpResponse>&) {
    return SuccessExecutionResult();
  };

  nghttp2::asio_http2::server::response response;
  std::shared_ptr<MockNgHttp2RequestWithOverrides> mock_http2_request =
      CreateMockRequest();
  auto mock_http2_response =
      std::make_shared<MockNgHttp2ResponseWithOverrides>(response);
  AsyncContext<NgHttp2Request, NgHttp2Response> ng_http2_context(
      mock_http2_request,
      [](AsyncContext<NgHttp2Request, NgHttp2Response>&) {});
  ng_http2_context.response = mock_http2_response;

  http_server.HandleHttp2Request(ng_http2_context, callback);
  std::shared_ptr<MockHttp2ServerWithOverrides::Http2SynchronizationContext>
      sync_context;
  EXPECT_EQ(http_server.GetActiveRequests().Find(ng_http2_context.request->id,
                                                 sync_context),
            SuccessExecutionResult());
  EXPECT_EQ(sync_context->failed.load(), false);
  EXPECT_EQ(sync_context->pending_callbacks.load(), 2);
  EXPECT_TRUE(mock_http2_request->IsOnRequestBodyDataReceivedCallbackSet());
}

TEST_F(Http2ServerTest, HandleHttp2RequestWithAwsProxy) {
  std::string host_address("localhost");
  std::string port("0");

  auto mock_authorization_proxy = std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      mock_authorization_proxy;
  std::shared_ptr<MockAuthorizationProxy> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  EXPECT_CALL(*mock_aws_authorization_proxy, Authorize)
      .WillOnce(Return(SuccessExecutionResult()));
  auto mock_metric_client = std::make_shared<MockMetricClient>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  MockHttp2ServerWithOverrides http_server(
      host_address, port, async_executor, authorization_proxy,
      mock_aws_authorization_proxy, mock_metric_client, mock_config_provider_);

  HttpHandler callback = [](AsyncContext<HttpRequest, HttpResponse>&) {
    return SuccessExecutionResult();
  };

  auto mock_http2_request = CreateMockRequest(Http2ServerTestCloud::kAws);

  nghttp2::asio_http2::server::response response;
  auto mock_http2_response =
      std::make_shared<MockNgHttp2ResponseWithOverrides>(response);

  AsyncContext<NgHttp2Request, NgHttp2Response> ng_http2_context(
      mock_http2_request,
      [](AsyncContext<NgHttp2Request, NgHttp2Response>&) {});
  ng_http2_context.response = mock_http2_response;

  http_server.HandleHttp2Request(ng_http2_context, callback);
  std::shared_ptr<MockHttp2ServerWithOverrides::Http2SynchronizationContext>
      sync_context;
  EXPECT_EQ(http_server.GetActiveRequests().Find(ng_http2_context.request->id,
                                                 sync_context),
            SuccessExecutionResult());
  EXPECT_EQ(sync_context->failed.load(), false);
  EXPECT_EQ(sync_context->pending_callbacks.load(), 2);
  EXPECT_TRUE(mock_http2_request->IsOnRequestBodyDataReceivedCallbackSet());
}

TEST_F(Http2ServerTest,
       HandleHttp2RequestSetsAuthorizedDomainFromAuthResponse) {
  std::string host_address("localhost");
  std::string port("0");

  auto mock_authorization_proxy = std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();

  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      mock_authorization_proxy;

  EXPECT_CALL(*mock_authorization_proxy, Authorize).WillOnce([](auto& context) {
    context.response = std::make_shared<AuthorizationProxyResponse>();
    context.response->authorized_metadata.authorized_domain =
        std::make_shared<std::string>("https://site.com");
    context.result = SuccessExecutionResult();

    context.Finish();
    return SuccessExecutionResult();
  });

  auto mock_metric_client = std::make_shared<MockMetricClient>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  std::shared_ptr<ConfigProviderInterface> config =
      std::make_shared<EnvConfigProvider>();
  MockHttp2ServerWithOverrides http_server(
      host_address, port, async_executor, authorization_proxy,
      mock_aws_authorization_proxy, mock_metric_client,
      std::make_shared<EnvConfigProvider>());
  EXPECT_SUCCESS(http_server.Init());
  HttpHandler callback = [](AsyncContext<HttpRequest, HttpResponse>&) {
    return SuccessExecutionResult();
  };

  nghttp2::asio_http2::server::response response;
  auto mock_http2_response =
      std::make_shared<MockNgHttp2ResponseWithOverrides>(response);

  std::shared_ptr<MockNgHttp2RequestWithOverrides> mock_http2_request =
      CreateMockRequest();
  AsyncContext<NgHttp2Request, NgHttp2Response> ng_http2_context(
      mock_http2_request,
      [](AsyncContext<NgHttp2Request, NgHttp2Response>&) {});
  ng_http2_context.response = mock_http2_response;

  http_server.HandleHttp2Request(ng_http2_context, callback);
  std::shared_ptr<MockHttp2ServerWithOverrides::Http2SynchronizationContext>
      sync_context;
  EXPECT_EQ(http_server.GetActiveRequests().Find(ng_http2_context.request->id,
                                                 sync_context),
            SuccessExecutionResult());
  EXPECT_EQ(sync_context->failed.load(), false);
  EXPECT_EQ(
      *sync_context->http2_context.request->auth_context.authorized_domain,
      "https://site.com");
}

TEST_F(Http2ServerTest, HandleHttp2RequestFailed) {
  std::string host_address("localhost");
  std::string port("0");

  auto mock_authorization_proxy = std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      mock_authorization_proxy;
  EXPECT_CALL(*mock_authorization_proxy, Authorize)
      .WillOnce(Return(FailureExecutionResult(123)));
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  auto mock_metric_client = std::make_shared<MockMetricClient>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  MockHttp2ServerWithOverrides http_server(
      host_address, port, async_executor, authorization_proxy,
      mock_aws_authorization_proxy, mock_metric_client, mock_config_provider_);

  HttpHandler callback = [](AsyncContext<HttpRequest, HttpResponse>&) {
    return SuccessExecutionResult();
  };

  bool should_continue = false;

  std::shared_ptr<MockNgHttp2RequestWithOverrides> mock_http2_request =
      CreateMockRequest();
  nghttp2::asio_http2::server::response response;

  AsyncContext<NgHttp2Request, NgHttp2Response> ng_http2_context(
      mock_http2_request, [&](AsyncContext<NgHttp2Request, NgHttp2Response>&) {
        should_continue = true;
      });
  ng_http2_context.response = std::make_shared<NgHttp2Response>(response);

  http_server.HandleHttp2Request(ng_http2_context, callback);
  http_server.OnHttp2Cleanup(ng_http2_context.parent_activity_id,
                             ng_http2_context.request->id, 0);

  std::shared_ptr<MockHttp2ServerWithOverrides::Http2SynchronizationContext>
      sync_context;
  EXPECT_EQ(
      http_server.GetActiveRequests().Find(ng_http2_context.request->id,
                                           sync_context),
      FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  WaitUntil([&]() { return should_continue; });
}

TEST_F(Http2ServerTest, TestOtelMetric) {
  std::shared_ptr<core::InMemoryMetricRouter> metric_router =
      std::make_shared<core::InMemoryMetricRouter>();

  // Setup the server and the client.
  std::shared_ptr<core::config_provider::mock::MockConfigProvider>
      mock_config_provider =
          std::make_shared<config_provider::mock::MockConfigProvider>();

  mock_config_provider->SetBool(kOtelServerMetricsEnabled, true);

  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      std::make_shared<PassThruAuthorizationProxy>();

  auto mock_metric_client = std::make_shared<MockMetricClient>();

  std::shared_ptr<AsyncExecutorInterface> async_executor_for_server_ =
      std::make_shared<AsyncExecutor>(/* thread pool size */ 20,
                                      /* queue size */ 100000,
                                      /* drop_tasks_on_stop */ true);

  std::shared_ptr<AsyncExecutorInterface> async_executor_for_client_ =
      std::make_shared<AsyncExecutor>(/* thread pool size */ 20,
                                      /* queue size */ 100000,
                                      /* drop_tasks_on_stop */ true);
  HttpClientOptions client_options(
      common::RetryStrategyOptions(common::RetryStrategyType::Linear,
                                   /* delay in ms */ 100, /* num retries */ 5),
      /* max connections per host */ 1, /* read timeout in sec */ 5);

  auto http2_client = std::make_shared<HttpClient>(
      async_executor_for_client_, client_options, metric_router);

  std::string host = "localhost";

  absl::BitGen generator;
  absl::uniform_int_distribution<int> distribution(1000, 9000);
  int random_port_number = distribution(generator);

  std::string port = std::to_string(random_port_number);

  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();

  std::shared_ptr<MockHttp2ServerWithOverrides> http_server =
      std::make_shared<MockHttp2ServerWithOverrides>(
          host, port, async_executor_for_server_, authorization_proxy,
          mock_aws_authorization_proxy, mock_metric_client,
          mock_config_provider);

  std::string path = "/v1/test";
  core::HttpHandler handler =
      [](AsyncContext<HttpRequest, HttpResponse>& context) {
        context.request->body = BytesBuffer("request body");
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer("response body");
        context.response->code = HttpStatusCode(200);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      };
  EXPECT_SUCCESS(http_server->RegisterResourceHandler(core::HttpMethod::POST,
                                                      path, handler));

  EXPECT_SUCCESS(async_executor_for_client_->Init());
  EXPECT_SUCCESS(async_executor_for_server_->Init());
  EXPECT_SUCCESS(http_server->Init());
  EXPECT_SUCCESS(http2_client->Init());

  EXPECT_SUCCESS(async_executor_for_client_->Run());
  EXPECT_SUCCESS(async_executor_for_server_->Run());
  EXPECT_SUCCESS(http_server->Run());
  EXPECT_SUCCESS(http2_client->Run());

  // Create a request and use the client to send it to the server.
  nghttp2::asio_http2::server::request request;
  nghttp2::asio_http2::server::response response;

  auto mock_http2_request =
      std::make_shared<MockNgHttp2RequestWithOverrides>(request);
  mock_http2_request->method = core::HttpMethod::POST;
  mock_http2_request->body = BytesBuffer("request body");
  mock_http2_request->path = std::make_shared<std::string>(
      absl::StrCat("http://", host, ":", port, "/v1/test"));
  mock_http2_request->headers = std::make_shared<HttpHeaders>();
  mock_http2_request->headers->insert(
      {kClaimedIdentityHeader, "https://origin.site.com"});
  auto mock_http2_response =
      std::make_shared<MockNgHttp2ResponseWithOverrides>(response);
  mock_http2_response->body = BytesBuffer("response body");

  absl::BlockingCounter blocking(1);
  AsyncContext<HttpRequest, HttpResponse> request_context(
      std::move(mock_http2_request),
      [&blocking](AsyncContext<HttpRequest, HttpResponse>& result_context) {
        blocking.DecrementCount();
      });
  request_context.response = mock_http2_response;

  EXPECT_SUCCESS(http2_client->PerformRequest(request_context));
  blocking.Wait();

  // Test otel metrics
  // Server latency
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router->GetExportedData();

  const std::map<std::string, std::string> server_latency_label_kv = {
      {kExecutionStatus, "Success"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      server_latency_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(server_latency_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      server_latency_metric_point_data = core::GetMetricPointData(
          kServerDurationMetric, server_latency_dimensions, data);

  EXPECT_TRUE(server_latency_metric_point_data.has_value());

  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          server_latency_metric_point_data.value()));

  // Active requests
  const std::map<std::string, std::string> active_requests_label_kv;
  const opentelemetry::sdk::common::OrderedAttributeMap
      active_requests_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(active_requests_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      active_requests_metric_point_data = core::GetMetricPointData(
          kActiveRequestsMetric, active_requests_dimensions, data);
  EXPECT_TRUE(active_requests_metric_point_data.has_value());

  auto active_requests_last_value_point_data =
      std::get<opentelemetry::sdk::metrics::LastValuePointData>(
          active_requests_metric_point_data.value());
  EXPECT_EQ(std::get<int64_t>(active_requests_last_value_point_data.value_), 0)
      << "Expected "
         "active_requests_last_value_point_data.value_ to be "
         "0 "
         "(int64_t)";

  // Request body size
  const std::map<std::string, std::string> request_body_label_kv;

  const opentelemetry::sdk::common::OrderedAttributeMap request_body_dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(request_body_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      request_body_metric_point_data = core::GetMetricPointData(
          kServerRequestBodySizeMetric, request_body_dimensions, data);

  EXPECT_TRUE(request_body_metric_point_data.has_value());

  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          request_body_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData request_body_histogram_data =
      reinterpret_cast<const opentelemetry::sdk::metrics::HistogramPointData&>(
          request_body_metric_point_data.value());

  auto request_body_histogram_data_max =
      std::get_if<int64_t>(&request_body_histogram_data.max_);
  EXPECT_EQ(*request_body_histogram_data_max, 12);

  // Response body size
  const std::map<std::string, std::string> response_body_label_kv = {
      {kResponseCode, "200"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      response_body_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(response_body_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      response_body_metric_point_data = core::GetMetricPointData(
          kServerResponseBodySizeMetric, response_body_dimensions, data);

  EXPECT_TRUE(response_body_metric_point_data.has_value());

  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          response_body_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData response_body_histogram_data =
      reinterpret_cast<const opentelemetry::sdk::metrics::HistogramPointData&>(
          response_body_metric_point_data.value());

  auto response_body_histogram_data_max =
      std::get_if<int64_t>(&response_body_histogram_data.max_);
  EXPECT_EQ(*response_body_histogram_data_max, 0);

  // Pbs transactions
  const std::map<std::string, std::string> pbs_transactions_label_kv = {
      {kResponseCode, "200"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      pbs_transactions_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(pbs_transactions_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      pbs_transactions_metric_point_data = core::GetMetricPointData(
          kPbsTransactionMetric, pbs_transactions_dimensions, data);

  EXPECT_TRUE(pbs_transactions_metric_point_data.has_value());

  EXPECT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      pbs_transactions_metric_point_data.value()));

  auto pbs_transactions_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          pbs_transactions_metric_point_data.value());

  EXPECT_EQ(std::get<int64_t>(pbs_transactions_sum_point_data.value_), 1)
      << "Expected pbs_transactions_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_SUCCESS(http2_client->Stop());
  EXPECT_SUCCESS(http_server->Stop());
  EXPECT_SUCCESS(async_executor_for_client_->Stop());
  EXPECT_SUCCESS(async_executor_for_server_->Stop());
}

TEST_F(Http2ServerTest, OnHttp2PendingCallbackFailure) {
  std::string host_address("localhost");
  std::string port("0");

  auto mock_authorization_proxy = std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      mock_authorization_proxy;
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  auto mock_metric_client = std::make_shared<MockMetricClient>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  MockHttp2ServerWithOverrides http_server(
      host_address, port, async_executor, authorization_proxy,
      mock_aws_authorization_proxy, mock_metric_client, mock_config_provider_);

  HttpHandler callback = [](AsyncContext<HttpRequest, HttpResponse>&) {
    return SuccessExecutionResult();
  };

  bool should_continue = false;

  std::shared_ptr<MockNgHttp2RequestWithOverrides> mock_http2_request =
      CreateMockRequest();
  nghttp2::asio_http2::server::response response;
  AsyncContext<NgHttp2Request, NgHttp2Response> ng_http2_context(
      mock_http2_request, [&](AsyncContext<NgHttp2Request, NgHttp2Response>&) {
        should_continue = true;
      });

  auto sync_context = std::make_shared<
      MockHttp2ServerWithOverrides::Http2SynchronizationContext>();
  sync_context->failed = false;
  sync_context->pending_callbacks = 2;
  sync_context->http2_context = ng_http2_context;
  sync_context->http_handler = callback;

  auto pair = make_pair(ng_http2_context.request->id, sync_context);
  EXPECT_SUCCESS(http_server.GetActiveRequests().Insert(pair, sync_context));

  auto callback_execution_result = FailureExecutionResult(1234);
  auto request_id = ng_http2_context.request->id;
  http_server.OnHttp2PendingCallback(callback_execution_result, request_id);
  WaitUntil([&]() { return should_continue; });

  EXPECT_SUCCESS(
      http_server.GetActiveRequests().Find(request_id, sync_context));
  EXPECT_EQ(sync_context->failed.load(), true);

  http_server.OnHttp2PendingCallback(callback_execution_result, request_id);
  http_server.OnHttp2Cleanup(sync_context->http2_context.parent_activity_id,
                             request_id, 0);
  EXPECT_THAT(http_server.GetActiveRequests().Find(request_id, sync_context),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
}

TEST_F(Http2ServerTest, OnHttp2PendingCallbackHttpHandlerFailure) {
  std::unique_ptr<core::InMemoryMetricRouter> metric_router =
      std::make_unique<core::InMemoryMetricRouter>();
  std::string host_address("localhost");
  std::string port("0");

  auto mock_authorization_proxy = std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      mock_authorization_proxy;
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  auto mock_metric_client = std::make_shared<MockMetricClient>();
  MockHttp2ServerWithOverrides http_server(
      host_address, port, async_executor, authorization_proxy,
      mock_aws_authorization_proxy, mock_metric_client, mock_config_provider_);

  ASSERT_SUCCESS(http_server.Init());

  HttpHandler callback = [](AsyncContext<HttpRequest, HttpResponse>&) {
    return FailureExecutionResult(12345);
  };

  bool should_continue = false;
  std::shared_ptr<MockNgHttp2RequestWithOverrides> mock_http2_request =
      CreateMockRequest();
  nghttp2::asio_http2::server::response response;
  AsyncContext<NgHttp2Request, NgHttp2Response> ng_http2_context(
      mock_http2_request,
      [&](AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context) {
        EXPECT_THAT(http2_context.result,
                    ResultIs(FailureExecutionResult(12345)));
        should_continue = true;
      });

  ng_http2_context.request->body = BytesBuffer("request body2");
  auto sync_context = std::make_shared<
      MockHttp2ServerWithOverrides::Http2SynchronizationContext>();
  sync_context->failed = false;
  sync_context->pending_callbacks = 1;
  sync_context->http2_context = ng_http2_context;
  sync_context->http_handler = callback;

  auto pair = make_pair(ng_http2_context.request->id, sync_context);
  EXPECT_SUCCESS(http_server.GetActiveRequests().Insert(pair, sync_context));

  auto callback_execution_result = SuccessExecutionResult();
  auto request_id = ng_http2_context.request->id;
  http_server.OnHttp2PendingCallback(callback_execution_result, request_id);
  WaitUntil([&]() { return should_continue; });

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router->GetExportedData();
  const std::map<std::string, std::string> request_body_label_kv = {};

  const opentelemetry::sdk::common::OrderedAttributeMap request_body_dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(request_body_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      request_body_metric_point_data = core::GetMetricPointData(
          kServerRequestBodySizeMetric, request_body_dimensions, data);

  EXPECT_TRUE(request_body_metric_point_data.has_value());

  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          request_body_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData request_body_histogram_data =
      reinterpret_cast<const opentelemetry::sdk::metrics::HistogramPointData&>(
          request_body_metric_point_data.value());

  auto request_body_histogram_data_max =
      std::get_if<int64_t>(&request_body_histogram_data.max_);
  EXPECT_EQ(*request_body_histogram_data_max, 13);
}

TEST_F(Http2ServerTest,
       ShouldFailToInitWhenTlsContextPrivateKeyFileDoesNotExist) {
  std::string host_address("localhost");
  std::string port("0");

  std::shared_ptr<AuthorizationProxyInterface> mock_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  size_t thread_pool_size = 2;

  Http2ServerOptions http2_server_options(
      true, std::make_shared<std::string>("/file/that/dos/not/exist.pem"),
      std::make_shared<std::string>("./public.crt"));

  Http2Server http_server(
      host_address, port, thread_pool_size, async_executor,
      mock_authorization_proxy, mock_aws_authorization_proxy,
      nullptr /* metric_client */, mock_config_provider_, http2_server_options);

  EXPECT_THAT(http_server.Init(),
              ResultIs(FailureExecutionResult(
                  errors::SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT)));
}

TEST_F(Http2ServerTest,
       ShouldFailToInitWhenTlsContextCertificateChainFileDoesNotExist) {
  std::string host_address("localhost");
  std::string port("0");

  std::shared_ptr<AuthorizationProxyInterface> mock_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  size_t thread_pool_size = 2;

  Http2ServerOptions http2_server_options(
      true, std::make_shared<std::string>("./privatekey.pem"),
      std::make_shared<std::string>("/file/that/dos/not/exist.crt"));

  Http2Server http_server(
      host_address, port, thread_pool_size, async_executor,
      mock_authorization_proxy, mock_aws_authorization_proxy,
      nullptr /* metric_client */, mock_config_provider_, http2_server_options);

  EXPECT_THAT(http_server.Init(),
              ResultIs(FailureExecutionResult(
                  errors::SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT)));
}

TEST_F(Http2ServerTest,
       ShouldInitCorrectlyWhenPrivateKeyAndCertChainFilesExist) {
  std::string host_address("localhost");
  std::string port("0");

  std::shared_ptr<AuthorizationProxyInterface> mock_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  size_t thread_pool_size = 2;

  Http2ServerOptions http2_server_options(
      true, std::make_shared<std::string>("./privatekey.pem"),
      std::make_shared<std::string>("./public.crt"));

  Http2Server http_server(
      host_address, port, thread_pool_size, async_executor,
      mock_authorization_proxy, mock_aws_authorization_proxy,
      nullptr /* metric_client */, mock_config_provider_, http2_server_options);

  EXPECT_SUCCESS(http_server.Init());
}

TEST_F(Http2ServerTest, ShouldInitCorrectlyRunAndStopWhenTlsIsEnabled) {
  std::string host_address("localhost");
  std::string port("0");

  std::shared_ptr<AuthorizationProxyInterface> mock_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  size_t thread_pool_size = 2;

  Http2ServerOptions http2_server_options(
      true, std::make_shared<std::string>("./privatekey.pem"),
      std::make_shared<std::string>("./public.crt"));

  Http2Server http_server(
      host_address, port, thread_pool_size, async_executor,
      mock_authorization_proxy, mock_aws_authorization_proxy,
      nullptr /* metric_client */, mock_config_provider_, http2_server_options);

  EXPECT_SUCCESS(http_server.Init());
  EXPECT_SUCCESS(http_server.Run());
  EXPECT_SUCCESS(http_server.Stop());
}

static int GenerateRandomIntInRange(int min, int max) {
  std::random_device randomdevice;
  std::mt19937 random_number_engine(randomdevice());
  std::uniform_int_distribution<int> uniform_distribution(min, max);

  return uniform_distribution(random_number_engine);
}

void SubmitUntilSuccess(HttpClient& http_client,
                        AsyncContext<HttpRequest, HttpResponse>& context) {
  ExecutionResult execution_result = RetryExecutionResult(123);
  while (execution_result.status == ExecutionStatus::Retry) {
    execution_result = http_client.PerformRequest(context);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  EXPECT_SUCCESS(execution_result);
}

TEST_F(Http2ServerTest, ShouldHandleRequestProperlyWhenTlsIsEnabled) {
  std::unique_ptr<core::InMemoryMetricRouter> metric_router =
      std::make_unique<core::InMemoryMetricRouter>();
  std::string host_address("localhost");
  int random_port = GenerateRandomIntInRange(8000, 60000);
  std::string port = std::to_string(random_port);
  std::shared_ptr<MockAuthorizationProxy> mock_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  std::shared_ptr<AuthorizationProxyInterface> mock_aws_authorization_proxy =
      std::make_shared<MockAuthorizationProxy>();
  EXPECT_CALL(*mock_authorization_proxy, Authorize).WillOnce([](auto& context) {
    context.response = std::make_shared<AuthorizationProxyResponse>();
    context.response->authorized_metadata.authorized_domain =
        std::make_shared<std::string>(
            context.request->authorization_metadata.claimed_identity);
    context.result = SuccessExecutionResult();

    context.Finish();
    return SuccessExecutionResult();
  });
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy =
      mock_authorization_proxy;
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<AsyncExecutor>(8, 10, true);

  size_t thread_pool_size = 2;
  std::string test_path("/test");

  Http2ServerOptions http2_server_options(
      true, std::make_shared<std::string>("./privatekey.pem"),
      std::make_shared<std::string>("./public.crt"));

  // Start the server
  Http2Server http_server(host_address, port, thread_pool_size, async_executor,
                          authorization_proxy, mock_aws_authorization_proxy,
                          nullptr /* metric_client */, mock_config_provider_,
                          http2_server_options);

  HttpHandler handler_callback =
      [](AsyncContext<HttpRequest, HttpResponse>& context) {
        context.result = SuccessExecutionResult();
        context.response->body = BytesBuffer("hello, world with TLS\r\n");
        context.Finish();
        return SuccessExecutionResult();
      };
  http_server.RegisterResourceHandler(HttpMethod::GET, test_path,
                                      handler_callback);

  EXPECT_SUCCESS(http_server.Init());
  EXPECT_SUCCESS(http_server.Run());

  // Start the client
  HttpClient http_client(async_executor);
  http_client.Init();
  http_client.Run();
  async_executor->Init();
  async_executor->Run();

  // Send request to server
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path =
      std::make_shared<std::string>("https://localhost:" + port + test_path);

  nlohmann::json json_token;
  std::string token_from_dumped_json = json_token.dump();
  std::string encoded_token;
  Base64Encode(token_from_dumped_json, encoded_token);
  request->headers = std::make_shared<HttpHeaders>();
  request->headers->insert({kAuthHeader, encoded_token});

  std::promise<void> done;
  AsyncContext<HttpRequest, HttpResponse> context(
      move(request), [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_SUCCESS(context.result);
        const auto& bytes = *context.response->body.bytes;
        EXPECT_EQ(std::string(bytes.begin(), bytes.end()),
                  "hello, world with TLS\r\n");
        done.set_value();
      });
  SubmitUntilSuccess(http_client, context);

  // Wait for request to be done.
  done.get_future().get();

  // Test empty request body collected.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router->GetExportedData();
  const std::map<std::string, std::string> request_body_label_kv;

  const opentelemetry::sdk::common::OrderedAttributeMap request_body_dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(request_body_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      request_body_metric_point_data = core::GetMetricPointData(
          kServerRequestBodySizeMetric, request_body_dimensions, data);

  EXPECT_TRUE(request_body_metric_point_data.has_value());

  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          request_body_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData request_body_histogram_data =
      reinterpret_cast<const opentelemetry::sdk::metrics::HistogramPointData&>(
          request_body_metric_point_data.value());

  auto request_body_histogram_data_max =
      std::get_if<int64_t>(&request_body_histogram_data.max_);
  EXPECT_EQ(*request_body_histogram_data_max, 0);

  http_client.Stop();
  http_server.Stop();
  async_executor->Stop();
}

TEST_F(Http2ServerTest,
       OnBodyDataReceivedWithExtraDataReturnsPartialDataError) {
  {
    nghttp2::asio_http2::server::request ng_request;
    MockNgHttp2RequestWithOverrides request(ng_request, 10 /* body length */);

    // Without callback to ensure nothing goes wrong.
    uint8_t data[11];
    request.SimulateOnRequestBodyDataReceived(data, 11);
  }
  {
    nghttp2::asio_http2::server::request ng_request;
    MockNgHttp2RequestWithOverrides request(ng_request, 10 /* body length */);

    // Install callback
    bool callback_called = false;
    request.SetOnRequestBodyDataReceivedCallback([&](ExecutionResult result) {
      EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                              errors::SC_HTTP2_SERVER_PARTIAL_REQUEST_BODY)));
      callback_called = true;
    });
    uint8_t data[11];
    request.SimulateOnRequestBodyDataReceived(data, 11);

    EXPECT_TRUE(callback_called);
  }
}

TEST_F(Http2ServerTest, OnBodyDataReceivedWithExactDataIsSuccessful) {
  {
    nghttp2::asio_http2::server::request ng_request;
    MockNgHttp2RequestWithOverrides request(ng_request, 10 /* body length */);

    // Without callback to ensure nothing goes wrong.
    uint8_t data[10];
    request.SimulateOnRequestBodyDataReceived(data, 10);
    request.SimulateOnRequestBodyDataReceived(data, 0);
  }
  {
    nghttp2::asio_http2::server::request ng_request;
    MockNgHttp2RequestWithOverrides request(ng_request, 10 /* body length */);

    // Install callback
    bool callback_called = false;
    request.SetOnRequestBodyDataReceivedCallback([&](ExecutionResult result) {
      EXPECT_SUCCESS(result);
      callback_called = true;
    });
    uint8_t data[11];
    request.SimulateOnRequestBodyDataReceived(data, 10);
    request.SimulateOnRequestBodyDataReceived(data, 0);

    EXPECT_TRUE(callback_called);
  }
}

TEST_F(Http2ServerTest, OnBodyDataReceivedWithLessDataReturnsPartialDataError) {
  {
    nghttp2::asio_http2::server::request ng_request;
    MockNgHttp2RequestWithOverrides request(ng_request, 10 /* body length */);

    // Without callback to ensure nothing goes wrong.
    uint8_t data[2];
    request.SimulateOnRequestBodyDataReceived(data, 2);
    request.SimulateOnRequestBodyDataReceived(data, 0);
  }
  {
    nghttp2::asio_http2::server::request ng_request;
    MockNgHttp2RequestWithOverrides request(ng_request, 10 /* body length */);

    // Install callback
    bool callback_called = false;
    request.SetOnRequestBodyDataReceivedCallback([&](ExecutionResult result) {
      EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                              errors::SC_HTTP2_SERVER_PARTIAL_REQUEST_BODY)));
      callback_called = true;
    });
    uint8_t data[11];
    request.SimulateOnRequestBodyDataReceived(data, 2);
    request.SimulateOnRequestBodyDataReceived(data, 0);

    EXPECT_TRUE(callback_called);
  }
}

}  // namespace
}  // namespace google::scp::core
