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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/mock/mock_authorization_proxy.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_forwarder/mock/mock_http2_forwarder.h"
#include "core/http2_server/mock/mock_http2_request_with_overrides.h"
#include "core/http2_server/mock/mock_http2_response_with_overrides.h"
#include "core/http2_server/mock/mock_http2_server_with_overrides.h"
#include "core/http2_server/src/error_codes.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/http_request_route_resolver_interface.h"
#include "core/interface/http_request_router_interface.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::HttpClient;
using google::scp::core::NgHttp2Request;
using google::scp::core::NgHttp2Response;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::authorization_proxy::mock::MockAuthorizationProxy;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::http2_forwarder::mock::MockHttp2Forwarder;
using google::scp::core::http2_server::mock::MockHttp2ServerWithOverrides;
using google::scp::core::http2_server::mock::MockNgHttp2RequestWithOverrides;
using google::scp::core::http2_server::mock::MockNgHttp2ResponseWithOverrides;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockMetricClient;
using std::make_shared;
using std::promise;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::chrono::milliseconds;
using std::chrono::seconds;
using testing::_;
using testing::Return;

namespace google::scp::core::test {

static int GenerateRandomIntInRange(int min, int max) {
  std::random_device randomdevice;
  std::mt19937 random_number_engine(randomdevice());
  std::uniform_int_distribution<int> uniform_distribution(min, max);

  return uniform_distribution(random_number_engine);
}

class MockRequestRouteResolver : public HttpRequestRouteResolverInterface {
 public:
  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD((ExecutionResultOr<RequestRouteEndpointInfo>), ResolveRoute,
              ((const HttpRequest&)), (noexcept, override));
};

class Http2RequestRouterTest : public testing::Test {
 protected:
  void SetUp() override {
    size_t thread_pool_size = 2;
    async_executor_ = std::make_shared<AsyncExecutor>(8, 10, true);
    shared_ptr<AuthorizationProxyInterface> mock_authorization_proxy =
        std::make_shared<MockAuthorizationProxy>();

    auto mock_metric_client = std::make_shared<MockMetricClient>();
    mock_config_provider_ = std::make_shared<MockConfigProvider>();
    mock_request_route_resolver_ = std::make_shared<MockRequestRouteResolver>();
    mock_request_router_ = std::make_shared<MockHttp2Forwarder>();

    std::shared_ptr<HttpRequestRouteResolverInterface> request_route_resolver =
        mock_request_route_resolver_;
    std::shared_ptr<HttpRequestRouterInterface> request_router =
        mock_request_router_;

    host_address_ = "localhost";
    port_ = to_string(GenerateRandomIntInRange(8000, 60000));

    http_server_ = std::make_shared<MockHttp2ServerWithOverrides>(
        host_address_, port_, thread_pool_size, async_executor_,
        mock_authorization_proxy, request_router, request_route_resolver,
        mock_metric_client, mock_config_provider_);
    // real HandleHttpRequest should not be invoked.
    http_server_->handle_http2_request_mock_ = [&](auto&, auto&) {};

    mock_server_request_with_overrides_ =
        std::make_shared<MockNgHttp2RequestWithOverrides>(request_);
    mock_server_response_with_overrides_ =
        std::make_shared<MockNgHttp2ResponseWithOverrides>(response_);
    server_request_context_.request = mock_server_request_with_overrides_;
    server_request_context_.callback = [&](auto& context) {};
    server_request_context_.response = mock_server_response_with_overrides_;
    server_request_context_.request->handler_path = "/v1/transactions:prepare";
  }

  std::shared_ptr<MockConfigProvider> mock_config_provider_;
  std::shared_ptr<AsyncExecutorInterface> async_executor_;
  std::shared_ptr<MockHttp2ServerWithOverrides> http_server_;
  std::shared_ptr<MockRequestRouteResolver> mock_request_route_resolver_;
  std::shared_ptr<MockHttp2Forwarder> mock_request_router_;
  std::string host_address_;
  std::string port_;

  nghttp2::asio_http2::server::request request_;
  nghttp2::asio_http2::server::response response_;
  shared_ptr<MockNgHttp2RequestWithOverrides>
      mock_server_request_with_overrides_;
  shared_ptr<MockNgHttp2ResponseWithOverrides>
      mock_server_response_with_overrides_;
  AsyncContext<NgHttp2Request, NgHttp2Response> server_request_context_;
  HttpHandler request_handler_;
};

TEST_F(Http2RequestRouterTest, RequestRoutingIsNotEnabledByDefault) {
  EXPECT_CALL(*mock_request_route_resolver_, ResolveRoute(_)).Times(0);
  EXPECT_CALL(*mock_request_router_, RouteRequest(_)).Times(0);
  http_server_->RouteOrHandleHttp2Request(server_request_context_,
                                          request_handler_);
}

TEST_F(Http2RequestRouterTest, FinishRequestWhenTheRequestCannotResolved) {
  mock_config_provider_->SetBool(kHTTPServerRequestRoutingEnabled, true);
  EXPECT_SUCCESS(http_server_->Init());

  EXPECT_CALL(*mock_request_route_resolver_, ResolveRoute(_))
      .WillOnce(Return(FailureExecutionResult(1234)));
  EXPECT_CALL(*mock_request_router_, RouteRequest(_)).Times(0);

  bool is_request_handled_locally = false;
  http_server_->handle_http2_request_mock_ = [&](auto&, auto&) {
    is_request_handled_locally = true;
  };

  bool context_finished = false;
  server_request_context_.callback = [&](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_HTTP2_SERVER_FAILED_TO_RESOLVE_ROUTE)));
    context_finished = true;
  };

  http_server_->RouteOrHandleHttp2Request(server_request_context_,
                                          request_handler_);
  EXPECT_TRUE(context_finished);
  EXPECT_FALSE(is_request_handled_locally);
}

TEST_F(Http2RequestRouterTest, HandlesLocalRequestWithRequestHandler) {
  mock_config_provider_->SetBool(kHTTPServerRequestRoutingEnabled, true);
  EXPECT_SUCCESS(http_server_->Init());

  RequestRouteEndpointInfo local_endpoint(
      std::make_shared<Uri>("https://localhost"), true /* is local endpoint */);
  EXPECT_CALL(*mock_request_route_resolver_, ResolveRoute(_))
      .WillOnce(Return(local_endpoint));
  EXPECT_CALL(*mock_request_router_, RouteRequest(_)).Times(0);

  std::atomic<bool> is_request_handled_locally = false;
  http_server_->handle_http2_request_mock_ = [&](auto& context, auto&) {
    is_request_handled_locally = true;
  };

  http_server_->RouteOrHandleHttp2Request(server_request_context_,
                                          request_handler_);
  EXPECT_TRUE(is_request_handled_locally);
}

TEST_F(Http2RequestRouterTest,
       FinishRequestWhenTheRequestDataCannotBeObtained) {
  mock_config_provider_->SetBool(kHTTPServerRequestRoutingEnabled, true);
  EXPECT_SUCCESS(http_server_->Init());

  RequestRouteEndpointInfo local_endpoint;

  std::atomic<bool> server_request_context_is_finished = false;
  server_request_context_.callback =
      [&server_request_context_is_finished](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        core::errors::SC_HTTP2_SERVER_PARTIAL_REQUEST_BODY)));
        server_request_context_is_finished = true;
      };

  http_server_->OnHttp2RequestDataObtainedRoutedRequest(
      server_request_context_, local_endpoint,
      FailureExecutionResult(
          core::errors::SC_HTTP2_SERVER_PARTIAL_REQUEST_BODY));
  EXPECT_TRUE(server_request_context_is_finished);
}

TEST_F(Http2RequestRouterTest, RemoteRequestIsRoutedSuccessfullyAndFinishes) {
  mock_config_provider_->SetBool(kHTTPServerRequestRoutingEnabled, true);
  EXPECT_SUCCESS(http_server_->Init());

  std::atomic<bool> is_request_handled_remotely = false;
  std::atomic<bool> server_request_context_is_finished = false;
  std::atomic<bool> is_request_handled_locally = false;

  RequestRouteEndpointInfo remote_endpoint(
      std::make_shared<Uri>("https://www.google.com"),
      false /* is local endpoint */);

  EXPECT_CALL(*mock_request_router_, RouteRequest(_))
      .WillOnce([&](AsyncContext<HttpRequest, HttpResponse> routing_context) {
        // Verify Request
        // Pointer is the same.
        EXPECT_EQ(routing_context.request, server_request_context_.request);
        EXPECT_EQ(*routing_context.request->path,
                  "https://www.google.com/v1/transactions:prepare");
        EXPECT_EQ(routing_context.response, nullptr);
        // Set Response
        routing_context.response = std::make_shared<HttpResponse>();
        routing_context.response->body = BytesBuffer("buf");
        routing_context.response->code = errors::HttpStatusCode::OK;
        routing_context.response->headers = std::make_shared<HttpHeaders>();
        routing_context.response->headers->insert({"key1", "val1"});
        FinishContext(SuccessExecutionResult(), routing_context);
        is_request_handled_remotely = true;
        return SuccessExecutionResult();
      });

  http_server_->handle_http2_request_mock_ = [&](auto& context, auto&) {
    FinishContext(SuccessExecutionResult(), context);
    is_request_handled_locally = true;
  };

  server_request_context_.callback =
      [&](AsyncContext<NgHttp2Request, NgHttp2Response>& context) {
        EXPECT_THAT(context.result, ResultIs(SuccessExecutionResult()));
        EXPECT_EQ(context.response->body.ToString(),
                  BytesBuffer("buf").ToString());
        EXPECT_EQ(context.response->code, errors::HttpStatusCode::OK);
        EXPECT_EQ(context.response->headers->size(), 1);
        EXPECT_EQ(context.response->headers->begin()->first, "key1");
        EXPECT_EQ(context.response->headers->begin()->second, "val1");
        server_request_context_is_finished = true;
      };

  // Invoke the data is received on the request, manually.
  http_server_->OnHttp2RequestDataObtainedRoutedRequest(
      server_request_context_, remote_endpoint, SuccessExecutionResult());

  EXPECT_TRUE(server_request_context_is_finished);
  EXPECT_FALSE(is_request_handled_locally);
  EXPECT_TRUE(is_request_handled_remotely);
}

TEST_F(Http2RequestRouterTest,
       FailsToSubmitRemoteRequestToRouterFinishesRequest) {
  mock_config_provider_->SetBool(kHTTPServerRequestRoutingEnabled, true);
  EXPECT_SUCCESS(http_server_->Init());

  std::atomic<bool> is_request_handled_remotely = false;
  std::atomic<bool> server_request_context_is_finished = false;
  std::atomic<bool> is_request_handled_locally = false;

  RequestRouteEndpointInfo remote_endpoint(
      std::make_shared<Uri>("https://www.google.com"),
      false /* is local endpoint */);

  EXPECT_CALL(*mock_request_router_, RouteRequest(_))
      .WillOnce([&](auto http_context) {
        is_request_handled_remotely = true;
        return FailureExecutionResult(1234);
      });

  http_server_->handle_http2_request_mock_ = [&](auto& context, auto&) {
    FinishContext(SuccessExecutionResult(), context);
    is_request_handled_locally = true;
  };

  server_request_context_.callback = [&](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_HTTP2_SERVER_FAILED_TO_ROUTE)));
    server_request_context_is_finished = true;
  };

  // Invoke the data is received on the request, manually.
  http_server_->OnHttp2RequestDataObtainedRoutedRequest(
      server_request_context_, remote_endpoint, SuccessExecutionResult());

  EXPECT_TRUE(server_request_context_is_finished);
  EXPECT_FALSE(is_request_handled_locally);
  EXPECT_TRUE(is_request_handled_remotely);
}

TEST_F(Http2RequestRouterTest,
       RequestRouterFailsToRouteRemoteRequestFinishesRequest) {
  mock_config_provider_->SetBool(kHTTPServerRequestRoutingEnabled, true);
  EXPECT_SUCCESS(http_server_->Init());

  std::atomic<bool> is_request_handled_remotely = false;
  std::atomic<bool> server_request_context_is_finished = false;
  std::atomic<bool> is_request_handled_locally = false;

  RequestRouteEndpointInfo remote_endpoint(
      std::make_shared<Uri>("https://www.google.com"),
      false /* is local endpoint */);

  EXPECT_CALL(*mock_request_router_, RouteRequest(_))
      .WillOnce([&](auto http_context) {
        FinishContext(FailureExecutionResult(1234), http_context);
        is_request_handled_remotely = true;
        return SuccessExecutionResult();
      });

  http_server_->handle_http2_request_mock_ = [&](auto& context, auto&) {
    FinishContext(SuccessExecutionResult(), context);
    is_request_handled_locally = true;
  };

  server_request_context_.callback = [&](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
    server_request_context_is_finished = true;
  };

  // Invoke the data is received on the request, manually.
  http_server_->OnHttp2RequestDataObtainedRoutedRequest(
      server_request_context_, remote_endpoint, SuccessExecutionResult());

  EXPECT_TRUE(server_request_context_is_finished);
  EXPECT_FALSE(is_request_handled_locally);
  EXPECT_TRUE(is_request_handled_remotely);
}

}  // namespace google::scp::core::test
