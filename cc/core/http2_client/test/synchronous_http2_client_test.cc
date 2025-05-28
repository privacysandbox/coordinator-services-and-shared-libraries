// Copyright 2025 Google LLC
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

#include "cc/core/http2_client/src/synchronous_http2_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <nghttp2/asio_http2_server.h>

#include "absl/random/random.h"
#include "cc/core/async_executor/mock/mock_async_executor.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "cc/core/common/operation_dispatcher/src/error_codes.h"
#include "cc/core/http2_client/src/error_codes.h"
#include "cc/core/http2_client/src/http_options.h"
#include "cc/core/http2_server/src/http2_server.h"
#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/errors.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {

class Http2ServerPeer {
 public:
  explicit Http2ServerPeer(std::unique_ptr<Http2Server> http2_server)
      : http2_server_(std::move(http2_server)) {}

  int PortInUse() { return http2_server_->PortInUse(); }

  ExecutionResult RegisterResourceHandler(HttpMethod http_method,
                                          std::string& path,
                                          HttpHandler& handler) noexcept {
    return http2_server_->RegisterResourceHandler(http_method, path, handler);
  }

  ExecutionResult Stop() { return http2_server_->Stop(); }

  ExecutionResult Init() { return http2_server_->Init(); }

  ExecutionResult Run() { return http2_server_->Run(); }

 private:
  std::unique_ptr<Http2Server> http2_server_;
};

namespace {
using ::nghttp2::asio_http2::server::http2;
using ::nghttp2::asio_http2::server::request;
using ::nghttp2::asio_http2::server::response;
using ::privacy_sandbox::pbs_common::AsyncExecutor;
using ::privacy_sandbox::pbs_common::Http2Server;
using ::privacy_sandbox::pbs_common::HttpStatusCode;
using ::privacy_sandbox::pbs_common::IsSuccessful;
using ::privacy_sandbox::pbs_common::MockAsyncExecutor;
using ::privacy_sandbox::pbs_common::PassThruAuthorizationProxy;
using ::privacy_sandbox::pbs_common::ResultIs;
using ::privacy_sandbox::pbs_common::RetryStrategyOptions;
using ::privacy_sandbox::pbs_common::RetryStrategyType;
using ::privacy_sandbox::pbs_common::TimeDuration;
using ::testing::AnyOf;

constexpr absl::string_view kHelloWorld = "hello, world\n";
constexpr absl::string_view kContentLengthHeader = "content-length";
constexpr absl::string_view kLength = "length";
constexpr absl::string_view kHashHeader = "hash";
constexpr absl::string_view kWrongPath = "/wrong";
constexpr absl::string_view kTestPath = "/test";
constexpr absl::string_view kPostEchoPath = "/post_echo";
constexpr absl::string_view kRandomPath = "/random";
constexpr absl::string_view k5xxErrorPath = "/error5xx";
constexpr absl::string_view k4xxErrorPath = "/error4xx";
constexpr TimeDuration kHttp2ReadTimeoutInSeconds = 10;
constexpr TimeDuration kRetryStrategyDelayInMs = 20;
constexpr size_t kRetryCount = 5;

std::string UniformRandomString(size_t len) {
  absl::BitGen bitgen;
  std::string bytes(len, '0');
  for (int i = 0; i < len; ++i) {
    bytes[i] = absl::Uniform<uint8_t>(bitgen);
  }
  return bytes;
}

std::unique_ptr<SyncHttpClient> MakeSyncHttpClient() {
  auto options = HttpClientOptions(
      RetryStrategyOptions(RetryStrategyType::Exponential,
                           kRetryStrategyDelayInMs, kRetryCount),
      kDefaultMaxConnectionsPerHost, kHttp2ReadTimeoutInSeconds);
  return std::make_unique<SyncHttpClient>(options);
}

class TestHttp2Server {
 public:
  TestHttp2Server(std::string host_address, std::string port, bool use_tls,
                  size_t thread_pool_size, size_t num_threads) {
    std::unique_ptr<Http2ServerOptions> http2_server_options =
        std::make_unique<Http2ServerOptions>();
    if (use_tls) {
      std::system("openssl genrsa 2048 > privatekey.pem");
      std::system(
          "openssl req -new -key privatekey.pem -out csr.pem -config "
          "cc/core/http2_server/test/certs/csr.conf");
      std::system(
          "openssl x509 -req -days 7305 -in csr.pem -signkey privatekey.pem "
          "-out "
          "public.crt");

      http2_server_options = std::make_unique<Http2ServerOptions>(
          true, std::make_shared<std::string>("./privatekey.pem"),
          std::make_shared<std::string>("./public.crt"));
    }

    async_executor_ = std::make_shared<AsyncExecutor>(num_threads, 10, true);
    authorization_proxy_ = std::make_shared<PassThruAuthorizationProxy>();

    auto http2_server_peer = std::make_unique<Http2Server>(
        host_address, port, thread_pool_size, async_executor_,
        authorization_proxy_,
        /*aws_authorization_proxy=*/nullptr, /*config_provider=*/nullptr,
        *http2_server_options,
        /*metric_router=*/nullptr);
    http2_server_peer_ =
        std::make_unique<Http2ServerPeer>(std::move(http2_server_peer));
  }

  ~TestHttp2Server() { http2_server_peer_->Stop(); }

  ExecutionResult Test(AsyncContext<HttpRequest, HttpResponse>& http_context) {
    auto& response = http_context.response;
    response->headers = std::make_shared<HttpHeaders>();
    response->headers->emplace("foo", "bar");
    response->body.bytes = std::make_shared<std::vector<Byte>>(
        kHelloWorld.begin(), kHelloWorld.end());
    response->body.capacity = kHelloWorld.size();
    response->body.length = kHelloWorld.size();
    http_context.result = SuccessExecutionResult();
    http_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult Random(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    auto& response = http_context.response;
    response->headers = std::make_shared<HttpHeaders>();

    auto length_it = http_context.request->headers->find(std::string(kLength));
    if (length_it == http_context.request->headers->end()) {
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_BAD_REQUEST);
    }
    size_t length = std::stoul(length_it->second);

    std::string str_resp = UniformRandomString(length);
    response->body.bytes =
        std::make_shared<std::vector<Byte>>(str_resp.begin(), str_resp.end());
    response->body.capacity = str_resp.size();
    response->body.length = str_resp.size();
    response->headers->emplace(kContentLengthHeader, std::to_string(length));
    response->headers->emplace(
        kHashHeader, std::to_string(std::hash<std::string>{}(str_resp)));
    http_context.result = SuccessExecutionResult();
    http_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult PostEcho(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    auto& response = http_context.response;
    response->headers = http_context.request->headers;
    response->body = http_context.request->body;
    http_context.result = SuccessExecutionResult();
    http_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult Return5XXError(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    // Return any error code that translates to 5xx error code that is retryable
    http_context.result = FailureExecutionResult(
        SC_HTTP2_CLIENT_HTTP_STATUS_INTERNAL_SERVER_ERROR);
    http_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult Return4XXError(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    // Copy the headers and the body
    auto& response = http_context.response;
    response->headers = http_context.request->headers;
    response->body = http_context.request->body;
    // Return any error code that translates to 4xx error code
    http_context.result =
        FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_PRECONDITION_FAILED);
    http_context.Finish();
    return SuccessExecutionResult();
  }

  void Run() {
    std::string test_path(kTestPath);
    HttpHandler test_handler = std::bind_front(&TestHttp2Server::Test, this);
    http2_server_peer_->RegisterResourceHandler(HttpMethod::GET, test_path,
                                                test_handler);

    std::string random_path(kRandomPath);
    HttpHandler random_handler =
        std::bind_front(&TestHttp2Server::Random, this);
    http2_server_peer_->RegisterResourceHandler(HttpMethod::GET, random_path,
                                                random_handler);

    std::string post_echo_path(kPostEchoPath);
    HttpHandler post_echo_handler =
        std::bind_front(&TestHttp2Server::PostEcho, this);
    http2_server_peer_->RegisterResourceHandler(
        HttpMethod::POST, post_echo_path, post_echo_handler);

    std::string error_5xx_path(k5xxErrorPath);
    HttpHandler error_5xx_handler =
        std::bind_front(&TestHttp2Server::Return5XXError, this);
    http2_server_peer_->RegisterResourceHandler(HttpMethod::GET, error_5xx_path,
                                                error_5xx_handler);

    std::string error_4xx_path(k4xxErrorPath);
    HttpHandler error_4xx_handler =
        std::bind_front(&TestHttp2Server::Return4XXError, this);
    http2_server_peer_->RegisterResourceHandler(
        HttpMethod::POST, error_4xx_path, error_4xx_handler);

    ASSERT_SUCCESS(http2_server_peer_->Init());
    ASSERT_SUCCESS(http2_server_peer_->Run());

    port = http2_server_peer_->PortInUse();
  }

  std::shared_ptr<AsyncExecutorInterface> async_executor_;
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy_;
  std::unique_ptr<Http2ServerPeer> http2_server_peer_;
  int port;
};

TEST(SyncHttpClientTestWithoutServer, FailedToConnect) {
  HttpRequest request{};
  request.method = HttpMethod::GET;
  request.path = std::make_shared<std::string>("http://localhost.failed:8000");

  auto http_client = MakeSyncHttpClient();

  auto response = http_client->PerformRequest(request);
  EXPECT_THAT(
      response.execution_result,
      AnyOf(ResultIs(FailureExecutionResult(SC_DISPATCHER_EXHAUSTED_RETRIES)),
            ResultIs(FailureExecutionResult(SC_DISPATCHER_OPERATION_EXPIRED))));
}

class SyncHttpClientTest : public ::testing::TestWithParam</*use_tls=*/bool> {
 protected:
  void SetUp() override {
    server = std::make_unique<TestHttp2Server>("localhost", "0", UseTls(),
                                               /*thread_pool_size=*/1,
                                               /*num_threads=*/1);
    server->Run();
    http_client = MakeSyncHttpClient();
  }

  std::string PortInUse() { return std::to_string(server->port); }

  bool UseTls() { return GetParam(); }

  std::string GetBasePath() {
    if (UseTls()) {
      return "https://localhost:" + PortInUse();
    } else {
      return "http://localhost:" + PortInUse();
    }
  }

  std::unique_ptr<TestHttp2Server> server;
  std::unique_ptr<SyncHttpClient> http_client;
};

INSTANTIATE_TEST_SUITE_P(SyncHttpClientTest, SyncHttpClientTest,
                         testing::Values(true, false));

TEST_P(SyncHttpClientTest, SuccessGet) {
  HttpRequest request{};
  request.method = HttpMethod::GET;
  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(kTestPath));

  auto response = http_client->PerformRequest(request);
  EXPECT_SUCCESS(response.execution_result);
  EXPECT_NE(response.http_response, nullptr);
  const auto& bytes = *response.http_response->body.bytes;
  EXPECT_EQ(std::string(bytes.begin(), bytes.end()), kHelloWorld);
}

TEST_P(SyncHttpClientTest, SuccessPost) {
  HttpRequest request{};
  request.method = HttpMethod::POST;
  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(kPostEchoPath));
  request.body.bytes = std::make_shared<std::vector<Byte>>();
  request.body.bytes->assign(kHelloWorld.begin(), kHelloWorld.end());
  request.body.capacity = kHelloWorld.size();
  request.body.length = kHelloWorld.size();
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->emplace("foo", "bar");

  auto response = http_client->PerformRequest(request);
  EXPECT_SUCCESS(response.execution_result);
  EXPECT_NE(response.http_response, nullptr);
  const auto& bytes = *response.http_response->body.bytes;
  EXPECT_EQ(std::string(bytes.begin(), bytes.end()), kHelloWorld);
  auto header_it = response.http_response->headers->find(std::string("foo"));
  EXPECT_NE(header_it, response.http_response->headers->end());
  EXPECT_EQ(header_it->second, "bar");
}

TEST_P(SyncHttpClientTest, FailedToGetResponse) {
  HttpRequest request{};
  request.method = HttpMethod::GET;
  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(kWrongPath));
  auto response = http_client->PerformRequest(request);
  EXPECT_THAT(
      response.execution_result,
      ResultIs(FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND)));
}

TEST_P(SyncHttpClientTest, SequentialReuse) {
  HttpRequest request{};
  request.method = HttpMethod::GET;
  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(kTestPath));

  for (int i = 0; i < 10; ++i) {
    auto response = http_client->PerformRequest(request);
    EXPECT_SUCCESS(response.execution_result);
    EXPECT_NE(response.http_response, nullptr);
    const auto& bytes = *response.http_response->body.bytes;
    EXPECT_EQ(std::string(bytes.begin(), bytes.end()), kHelloWorld);
  }
}

TEST_P(SyncHttpClientTest, ConcurrentReuse) {
  HttpRequest request{};
  request.method = HttpMethod::GET;
  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(kTestPath));

  std::vector<std::jthread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&]() {
      auto response = http_client->PerformRequest(request);
      EXPECT_SUCCESS(response.execution_result);
      EXPECT_NE(response.http_response, nullptr);
      const auto& bytes = *response.http_response->body.bytes;
      EXPECT_EQ(std::string(bytes.begin(), bytes.end()), kHelloWorld);
    });
  }
}

TEST_P(SyncHttpClientTest, LargeData) {
  HttpRequest request{};
  request.method = HttpMethod::GET;
  size_t to_generate = 1048576UL;
  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(kRandomPath));
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->emplace(kLength, std::to_string(to_generate));

  auto response = http_client->PerformRequest(request);
  EXPECT_SUCCESS(response.execution_result);
  EXPECT_NE(response.http_response, nullptr);
  EXPECT_EQ(response.http_response->body.length, 1048576);
  std::string resp_str(response.http_response->body.bytes->begin(),
                       response.http_response->body.bytes->end());
  auto resp_hash = std::hash<std::string>{}(resp_str);
  auto hash_it =
      response.http_response->headers->find(std::string(kHashHeader));
  EXPECT_NE(hash_it, response.http_response->headers->end());
  EXPECT_EQ(std::to_string(resp_hash), hash_it->second);
}

TEST_P(SyncHttpClientTest, ClientFinishesContextWhenServerIsStopped) {
  HttpRequest request{};
  request.method = HttpMethod::GET;

  {
    request.path =
        std::make_shared<std::string>(GetBasePath() + std::string(kTestPath));
    auto response = http_client->PerformRequest(request);
    EXPECT_SUCCESS(response.execution_result);
    EXPECT_NE(response.http_response, nullptr);
    const auto& bytes = *response.http_response->body.bytes;
    EXPECT_EQ(std::string(bytes.begin(), bytes.end()), kHelloWorld);
  }

  server->http2_server_peer_->Stop();

  {
    request.path =
        std::make_shared<std::string>(GetBasePath() + std::string(kTestPath));
    auto response = http_client->PerformRequest(request);
    EXPECT_THAT(
        response.execution_result,
        AnyOf(
            ResultIs(FailureExecutionResult(SC_DISPATCHER_EXHAUSTED_RETRIES)),
            ResultIs(FailureExecutionResult(
                SC_DISPATCHER_NOT_ENOUGH_TIME_REMAINED_FOR_OPERATION)),
            ResultIs(FailureExecutionResult(SC_DISPATCHER_OPERATION_EXPIRED))));
  }
}

TEST_P(SyncHttpClientTest, ConnectionCreationFailure) {
  HttpRequest request{};
  request.method = HttpMethod::GET;

  request.path = std::make_shared<std::string>(
      "http$://localhost:" + PortInUse() + std::string(kTestPath));
  auto response = http_client->PerformRequest(request);
  EXPECT_EQ(response.http_response, nullptr);
  EXPECT_THAT(response.execution_result,
              ResultIs(FailureExecutionResult(SC_HTTP2_CLIENT_INVALID_URI)));
}

TEST_P(SyncHttpClientTest, TestRetries) {
  HttpRequest request{};
  request.method = HttpMethod::GET;

  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(k5xxErrorPath));
  auto response = http_client->PerformRequest(request);
  EXPECT_FALSE(response.execution_result.Successful());
  EXPECT_THAT(
      response.execution_result,
      AnyOf(ResultIs(FailureExecutionResult(SC_DISPATCHER_EXHAUSTED_RETRIES)),
            ResultIs(FailureExecutionResult(SC_DISPATCHER_OPERATION_EXPIRED))));
}

TEST_P(SyncHttpClientTest, Test4xxError) {
  HttpRequest request{};
  request.method = HttpMethod::POST;

  request.path =
      std::make_shared<std::string>(GetBasePath() + std::string(k4xxErrorPath));
  request.body.bytes = std::make_shared<std::vector<Byte>>();
  request.body.bytes->assign(kHelloWorld.begin(), kHelloWorld.end());
  request.body.capacity = kHelloWorld.size();
  request.body.length = kHelloWorld.size();
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->emplace("foo", "bar");

  auto response = http_client->PerformRequest(request);
  EXPECT_FALSE(response.execution_result.Successful());
  EXPECT_NE(response.http_response, nullptr);

  const auto& bytes = *response.http_response->body.bytes;
  EXPECT_EQ(std::string(bytes.begin(), bytes.end()), kHelloWorld);
  auto header_it = response.http_response->headers->find(std::string("foo"));
  EXPECT_NE(header_it, response.http_response->headers->end());
  EXPECT_EQ(header_it->second, "bar");
}

}  // namespace
}  // namespace privacy_sandbox::pbs_common
