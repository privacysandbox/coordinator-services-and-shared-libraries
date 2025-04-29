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

#include "cc/core/http2_client/src/http2_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <future>
#include <memory>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <nghttp2/asio_http2_server.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "cc/core/async_executor/mock/mock_async_executor.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/http2_client/src/error_codes.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/telemetry/mock/in_memory_metric_router.h"
#include "cc/core/telemetry/src/common/metric_utils.h"
#include "cc/core/test/utils/conditional_wait.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {
namespace {
using ::google::scp::core::ExecutionStatus;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::GetMetricPointData;
using ::google::scp::core::InMemoryMetricRouter;
using ::privacy_sandbox::pbs_common::RetryStrategyOptions;
using ::privacy_sandbox::pbs_common::RetryStrategyType;
using ::google::scp::core::test::IsSuccessful;
using ::google::scp::core::test::ResultIs;
using ::nghttp2::asio_http2::server::http2;
using ::nghttp2::asio_http2::server::request;
using ::nghttp2::asio_http2::server::response;
using ::privacy_sandbox::pbs_common::WaitUntil;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

static constexpr TimeDuration kHttp2ReadTimeoutInSeconds = 10;

class RandomGenHandler : std::enable_shared_from_this<RandomGenHandler> {
 public:
  explicit RandomGenHandler(size_t length) : remaining_len_(length) {
    SHA256_Init(&sha256_ctx_);
  }

  ssize_t handle(uint8_t* data, size_t len, uint32_t* data_flags) {
    if (remaining_len_ == 0) {
      uint8_t hash[SHA256_DIGEST_LENGTH];
      SHA256_Final(hash, &sha256_ctx_);
      memcpy(data, hash, sizeof(hash));
      *data_flags |= NGHTTP2_DATA_FLAG_EOF;
      return sizeof(hash);
    }
    size_t to_generate = len < remaining_len_ ? len : remaining_len_;
    RAND_bytes(data, to_generate);
    SHA256_Update(&sha256_ctx_, data, to_generate);
    remaining_len_ -= to_generate;
    return to_generate;
  }

 private:
  SHA256_CTX sha256_ctx_;
  size_t remaining_len_;
};

class HttpServer {
 public:
  HttpServer(std::string address, std::string port, size_t num_threads)
      : address_(address), port_(port), num_threads_(num_threads) {}

  ~HttpServer() { server.join(); }

  void Run() {
    boost::system::error_code ec;

    server.num_threads(num_threads_);

    server.handle("/stop",
                  [this](const request& req, const response& res) { Stop(); });

    server.handle("/test", [](const request& req, const response& res) {
      res.write_head(200, {{"foo", {"bar"}}});
      res.end("hello, world\n");
    });

    server.handle(
        "/pingpong_query_param", [](const request& req, const response& res) {
          res.write_head(200, {{"query_param", {req.uri().raw_query.c_str()}}});
          res.end("hello, world\n");
        });

    server.handle("/random", [](const request& req, const response& res) {
      const auto& query = req.uri().raw_query;
      if (query.empty()) {
        res.write_head(400u);
        res.end();
        return;
      }
      std::vector<std::string> params;
      static auto predicate = [](std::string::value_type c) {
        return c == '=';
      };
      boost::split(params, query, predicate);
      if (params.size() != 2 || params[0] != "length") {
        res.write_head(400u);
        res.end();
        return;
      }
      size_t length = std::stoul(params[1]);
      if (length == 0) {
        res.write_head(400u);
        res.end();
        return;
      }

      res.write_head(
          200u, {{std::string("content-length"),
                  {std::to_string(length + SHA256_DIGEST_LENGTH), false}}});
      auto handler = std::make_shared<RandomGenHandler>(length);
      res.end(bind(&RandomGenHandler::handle, handler, _1, _2, _3));
    });

    server.listen_and_serve(ec, address_, port_, true);

    is_running_ = true;
  }

  void Stop() {
    if (is_running_) {
      is_running_ = false;
      server.stop();
    }
  }

  int PortInUse() { return server.ports()[0]; }

  http2 server;

 private:
  std::atomic<bool> is_running_{false};
  std::string address_;
  std::string port_;
  size_t num_threads_;
};

TEST(HttpClientTest, FailedToConnect) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>("http://localhost.failed:8000");
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<MockAsyncExecutor>();
  HttpClient http_client(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client.Init();
  http_client.Run();

  std::atomic<bool> finished(false);
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(

                        SC_DISPATCHER_NOT_ENOUGH_TIME_REMAINED_FOR_OPERATION)));
        finished.store(true);
      });

  EXPECT_SUCCESS(http_client.PerformRequest(context));
  WaitUntil([&]() { return finished.load(); });
  http_client.Stop();
  async_executor->Stop();
}

class HttpClientTestII : public ::testing::Test {
 protected:
  void SetUp() override {
    metric_router_ = std::make_unique<InMemoryMetricRouter>();
    server = std::make_shared<HttpServer>("localhost", "0", 1);
    server->Run();
    async_executor = std::make_shared<AsyncExecutor>(2, 1000);
    async_executor->Init();
    async_executor->Run();

    auto options = HttpClientOptions(
        RetryStrategyOptions(RetryStrategyType::Exponential,
                             kDefaultRetryStrategyDelayInMs,
                             kDefaultRetryStrategyMaxRetries),
        kDefaultMaxConnectionsPerHost, kHttp2ReadTimeoutInSeconds);

    http_client = std::make_shared<HttpClient>(async_executor, options,
                                               metric_router_.get());
    EXPECT_SUCCESS(http_client->Init());
    EXPECT_SUCCESS(http_client->Run());
  }

  void TearDown() override {
    EXPECT_SUCCESS(http_client->Stop());
    async_executor->Stop();
    server->Stop();
  }

  std::shared_ptr<HttpServer> server;
  std::shared_ptr<AsyncExecutorInterface> async_executor;
  std::shared_ptr<HttpClient> http_client;
  std::unique_ptr<InMemoryMetricRouter> metric_router_;
};

TEST_F(HttpClientTestII, Success) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/test");
  std::promise<void> done;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_SUCCESS(context.result);
        const auto& bytes = *context.response->body.bytes;
        EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "hello, world\n");
        done.set_value();
      });

  EXPECT_SUCCESS(http_client->PerformRequest(context));

  done.get_future().get();
}

TEST_F(HttpClientTestII, SingleQueryIsEscaped) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<Uri>(
      "http://localhost:" + std::to_string(server->PortInUse()) +
      "/pingpong_query_param");
  request->query = std::make_shared<std::string>("foo=!@#$");

  std::atomic<bool> finished(false);
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_SUCCESS(context.result);
        auto query_param_it = context.response->headers->find("query_param");
        EXPECT_NE(query_param_it, context.response->headers->end());
        EXPECT_EQ(query_param_it->second, "foo=%21%40%23%24");
        finished.store(true);
      });

  EXPECT_SUCCESS(http_client->PerformRequest(context));
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(HttpClientTestII, MultiQueryIsEscaped) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<Uri>(
      "http://localhost:" + std::to_string(server->PortInUse()) +
      "/pingpong_query_param");
  request->query = std::make_shared<std::string>("foo=!@#$&bar=%^()");

  std::atomic<bool> finished(false);
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_SUCCESS(context.result);
        auto query_param_it = context.response->headers->find("query_param");
        EXPECT_NE(query_param_it, context.response->headers->end());
        EXPECT_EQ(query_param_it->second, "foo=%21%40%23%24&bar=%25%5E%28%29");
        finished.store(true);
      });

  EXPECT_SUCCESS(http_client->PerformRequest(context));
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(HttpClientTestII, FailedToGetResponse) {
  auto request = std::make_shared<HttpRequest>();
  // Get has no corresponding handler.
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/wrong");
  std::promise<void> done;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(

                        SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND)));
        done.set_value();
      });

  EXPECT_SUCCESS(http_client->PerformRequest(context));
  done.get_future().get();
}

TEST_F(HttpClientTestII, SequentialReuse) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/test");

  for (int i = 0; i < 10; ++i) {
    std::promise<void> done;
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&](AsyncContext<HttpRequest, HttpResponse>& context) {
          EXPECT_SUCCESS(context.result);
          const auto& bytes = *context.response->body.bytes;
          EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "hello, world\n");
          done.set_value();
        });
    EXPECT_SUCCESS(http_client->PerformRequest(context));
    done.get_future().get();
  }
}

TEST_F(HttpClientTestII, ConcurrentReuse) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/test");
  std::shared_ptr<AsyncExecutorInterface> async_executor =
      std::make_shared<AsyncExecutor>(2, 1000);

  std::vector<std::promise<void>> done;
  done.reserve(10);
  for (int i = 0; i < 10; ++i) {
    done.emplace_back();
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&, i](AsyncContext<HttpRequest, HttpResponse>& context) {
          EXPECT_SUCCESS(context.result);
          const auto& bytes = *context.response->body.bytes;
          EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "hello, world\n");
          done[i].set_value();
        });
    EXPECT_SUCCESS(http_client->PerformRequest(context));
  }
  for (auto& p : done) {
    p.get_future().get();
  }
}

// Request /random?length=xxxx and verify the hash of the return.
TEST_F(HttpClientTestII, LargeData) {
  auto request = std::make_shared<HttpRequest>();
  size_t to_generate = 1048576UL;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/random");
  request->query =
      std::make_shared<std::string>("length=" + std::to_string(to_generate));
  std::atomic<bool> finished(false);
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->body.length,
                  1048576 + SHA256_DIGEST_LENGTH);
        uint8_t hash[SHA256_DIGEST_LENGTH];
        const auto* data = reinterpret_cast<const uint8_t*>(
            context.response->body.bytes->data());
        SHA256(data, to_generate, hash);
        auto ret = memcmp(hash, data + to_generate, SHA256_DIGEST_LENGTH);
        EXPECT_EQ(ret, 0);
        finished.store(true);
      });

  EXPECT_SUCCESS(http_client->PerformRequest(context));
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(HttpClientTestII, ClientFinishesContextWhenServerIsStopped) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;

  // Make success http request.
  {
    request->path = std::make_shared<std::string>(
        "http://localhost:" + std::to_string(server->PortInUse()) + "/test");
    std::promise<void> done;
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&](AsyncContext<HttpRequest, HttpResponse>& context) {
          EXPECT_THAT(context.result, IsSuccessful());
          const auto& bytes = *context.response->body.bytes;
          EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "hello, world\n");
          done.set_value();
        });

    EXPECT_THAT(http_client->PerformRequest(context), IsSuccessful());
    done.get_future().get();
  }

  // Http context will be finished correctly even the http server stopped.
  {
    request->path = std::make_shared<std::string>(
        "http://localhost:" + std::to_string(server->PortInUse()) + "/stop");

    std::promise<void> done;
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&](AsyncContext<HttpRequest, HttpResponse>& context) {
          EXPECT_THAT(
              context.result,
              ResultIs(FailureExecutionResult(

                  SC_DISPATCHER_NOT_ENOUGH_TIME_REMAINED_FOR_OPERATION)));
          done.set_value();
        });
    EXPECT_THAT(http_client->PerformRequest(context), IsSuccessful());
    done.get_future().get();
  }
}

TEST_F(HttpClientTestII, ConnectionCreationFailure) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>(
      "http$://localhost:" + std::to_string(server->PortInUse()) + "/test");
  std::promise<void> done;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Failure);
        done.set_value();
      });

  EXPECT_SUCCESS(http_client->PerformRequest(context));

  done.get_future().get();

  // Test otel metrics.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string>
      client_connection_creation_error_label_kv;

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_connection_creation_error_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_connection_creation_error_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_connection_creation_error_metric_point_data =
          GetMetricPointData("http.client.connection.creation_errors",
                             client_connection_creation_error_dimensions, data);

  EXPECT_TRUE(client_connection_creation_error_metric_point_data.has_value());

  EXPECT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      client_connection_creation_error_metric_point_data.value()));

  auto client_connection_creation_error_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          client_connection_creation_error_metric_point_data.value());

  EXPECT_EQ(
      std::get<int64_t>(client_connection_creation_error_sum_point_data.value_),
      1)
      << "Expected client_connection_creation_error_sum_point_data.value_ to "
         "be 1 "
         "(int64_t)";
}
}  // namespace
}  // namespace privacy_sandbox::pbs_common
