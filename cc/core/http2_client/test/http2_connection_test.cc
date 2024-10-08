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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <csignal>
#include <memory>
#include <vector>

#include <nghttp2/asio_http2_server.h>

#include "absl/synchronization/blocking_counter.h"
#include "cc/core/interface/async_context.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/http2_client/mock/mock_http_connection.h"
#include "core/http2_client/src/http_client_def.h"
#include "core/telemetry/mock/in_memory_metric_router.h"
#include "core/telemetry/src/common/metric_utils.h"
#include "core/test/utils/conditional_wait.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using ::google::scp::core::AsyncExecutor;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::async_executor::mock::MockAsyncExecutor;
using ::google::scp::core::common::Uuid;
using ::google::scp::core::http2_client::mock::MockHttpConnection;
using ::google::scp::core::test::IsSuccessful;
using ::google::scp::core::test::ResultIs;
using ::google::scp::core::test::WaitUntil;
using ::opentelemetry::sdk::resource::SemanticConventions::
    kHttpResponseStatusCode;
using ::opentelemetry::sdk::resource::SemanticConventions::kServerAddress;
using ::testing::IsEmpty;

namespace google::scp::core {
namespace {

class HttpConnectionTest : public testing::Test {
 protected:
  void SetUp() override {
    server_.num_threads(1);

    boost::system::error_code ec;

    server_.listen_and_serve(ec, /*address=*/"localhost",
                             /*port=*/"0", /*asynchronous=*/true);
    ASSERT_FALSE(ec) << "Failed to start the server: " << ec.message();

    metric_router_ = std::make_unique<core::InMemoryMetricRouter>();
  }

  void TearDown() override {
    server_.stop();
    server_.join();
  }

  nghttp2::asio_http2::server::http2 server_;
  std::unique_ptr<core::InMemoryMetricRouter> metric_router_;
};

TEST_F(HttpConnectionTest, SimpleRequest) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Success);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();
  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  // Test otel client response metric.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> client_response_label_kv = {
      {std::string(kHttpResponseStatusCode), "200"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_response_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(client_response_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_response_metric_point_data = core::GetMetricPointData(
          kClientResponseMetric, client_response_dimensions, data);

  ASSERT_TRUE(client_response_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      client_response_metric_point_data.value()));

  auto client_response_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          client_response_metric_point_data.value());

  EXPECT_EQ(std::get<int64_t>(client_response_sum_point_data.value_), 1)
      << "Expected client_response_sum_point_data.value_ to be 1 (int64_t)";
}

TEST_F(HttpConnectionTest, CancelCallbacks) {
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {});

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  bool is_called = false;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (!is_called) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
          counter.DecrementCount();
        }
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  EXPECT_EQ(is_called, false);
  connection.CancelPendingCallbacks();
  counter.Wait();
  EXPECT_EQ(is_called, true);

  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  connection.Stop();
}

TEST_F(HttpConnectionTest, StopRemovesCallback) {
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {});

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  bool is_called = false;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (!is_called) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
          counter.DecrementCount();
        }
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  while (keys.empty()) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  EXPECT_EQ(is_called, false);
  connection.Stop();
  counter.Wait();

  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());
  EXPECT_EQ(is_called, true);
}

TEST_F(HttpConnectionTest, SuccessfulImmediateResponse) {
  server_.handle(/*pattern=*/"/success",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   res.write_head(200);
                   res.end("Success");
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/success");
  http_context.request->method = HttpMethod::GET;

  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->body.ToString(), "Success");
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  counter.Wait();

  connection.Stop();

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());
}

TEST_F(HttpConnectionTest, ServerThrewInternalServerError) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/error",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(500);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/error");
  http_context.request->method = HttpMethod::GET;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.response.get()->code,
                  errors::HttpStatusCode::INTERNAL_SERVER_ERROR);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  release_response.DecrementCount();

  counter.Wait();
  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  // Test otel client response metric.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> client_response_label_kv = {
      {std::string(kHttpResponseStatusCode), "500"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_response_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(client_response_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_response_metric_point_data = core::GetMetricPointData(
          kClientResponseMetric, client_response_dimensions, data);

  EXPECT_TRUE(client_response_metric_point_data.has_value());

  EXPECT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      client_response_metric_point_data.value()));

  auto client_response_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          client_response_metric_point_data.value());

  EXPECT_EQ(std::get<int64_t>(client_response_sum_point_data.value_), 1)
      << "Expected client_response_sum_point_data.value_ to be 1 (int64_t)";
}

TEST_F(HttpConnectionTest, UnsupportedHttpMethod) {
  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::PUT;  // Unsupported method
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Failure);
        EXPECT_EQ(context.result.status_code,
                  errors::SC_HTTP2_CLIENT_HTTP_METHOD_NOT_SUPPORTED);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  counter.Wait();
  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());
}

TEST_F(HttpConnectionTest, MissingHeadersShouldStillSuccess) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  http_context.request->headers.reset();  // Simulate missing headers
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Success);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();
  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());
}

TEST_F(HttpConnectionTest, RequestSubmissionFailure) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http:/invalid-uri");
  http_context.request->method = HttpMethod::GET;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Retry);
        EXPECT_EQ(context.result.status_code,
                  errors::SC_HTTP2_CLIENT_FAILED_TO_ISSUE_HTTP_REQUEST);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();
  execution_result =
      test::WaitUntilOrReturn([&]() { return !connection.IsReady(); });
  ASSERT_SUCCESS(execution_result) << "Connection has not be dropped.";
  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());
}

TEST_F(HttpConnectionTest, ClientServerLatencyMeasurement) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Success);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();

  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  // Test otel metrics.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> client_server_latency_label_kv = {
      {std::string(kServerAddress), "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_server_latency_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_server_latency_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_server_latency_metric_point_data = core::GetMetricPointData(
          kClientServerLatencyMetric, client_server_latency_dimensions, data);

  ASSERT_TRUE(client_server_latency_metric_point_data.has_value());

  ASSERT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          client_server_latency_metric_point_data.value()));
}

TEST_F(HttpConnectionTest, ClientRequestDurationMeasurement) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Success);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();

  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  // Test otel metrics.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> client_request_duration_label_kv = {
      {std::string(kServerAddress), "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_request_duration_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_request_duration_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_request_duration_metric_point_data =
          core::GetMetricPointData(kClientRequestDurationMetric,
                                   client_request_duration_dimensions, data);

  ASSERT_TRUE(client_request_duration_metric_point_data.has_value());

  ASSERT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          client_request_duration_metric_point_data.value()));
}

TEST_F(HttpConnectionTest, ClientConnectionDurationMeasurement) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Success);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();

  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  // Test otel metrics.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> client_connection_duration_label_kv =
      {{std::string(kServerAddress), "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_connection_duration_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_connection_duration_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_connection_duration_metric_point_data =
          core::GetMetricPointData(kClientConnectionDurationMetric,
                                   client_connection_duration_dimensions, data);

  ASSERT_TRUE(client_connection_duration_metric_point_data.has_value());

  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          client_connection_duration_metric_point_data.value()));
}

TEST_F(HttpConnectionTest, ClientConnectionError) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http:/invalid-uri");
  http_context.request->method = HttpMethod::GET;
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Retry);
        EXPECT_EQ(context.result.status_code,
                  errors::SC_HTTP2_CLIENT_FAILED_TO_ISSUE_HTTP_REQUEST);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();
  counter.Wait();

  execution_result =
      test::WaitUntilOrReturn([&]() { return !connection.IsReady(); });
  ASSERT_SUCCESS(execution_result) << "Connection has not be dropped.";

  // Test otel metrics.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  // Connection Duration
  const std::map<std::string, std::string> client_connection_duration_label_kv =
      {{std::string(kServerAddress), "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_connection_duration_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_connection_duration_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_connection_duration_metric_point_data =
          core::GetMetricPointData(kClientConnectionDurationMetric,
                                   client_connection_duration_dimensions, data);

  ASSERT_TRUE(client_connection_duration_metric_point_data.has_value());

  ASSERT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          client_connection_duration_metric_point_data.value()));

  // Connection Error
  const std::map<std::string, std::string> client_connect_error_label_kv = {
      {std::string(kServerAddress), "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_connect_error_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_connect_error_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_connect_error_metric_point_data = core::GetMetricPointData(
          kClientConnectErrorsMetric, client_connect_error_dimensions, data);

  ASSERT_TRUE(client_connect_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      client_connect_error_metric_point_data.value()));

  auto client_connect_error_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          client_connect_error_metric_point_data.value());

  EXPECT_EQ(std::get<int64_t>(client_connect_error_sum_point_data.value_), 1)
      << "Expected client_connect_error_sum_point_data.value_ to be 1 "
         "(int64_t)";

  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());
}

TEST_F(HttpConnectionTest, RequestResponseBodySizeMeasurement) {
  absl::BlockingCounter release_response(1);
  server_.handle(/*pattern=*/"/test",
                 [&](const nghttp2::asio_http2::server::request& req,
                     const nghttp2::asio_http2::server::response& res) {
                   release_response.Wait();
                   res.write_head(200);
                   res.end();
                 });

  auto async_executor =
      std::make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/20);
  MockHttpConnection connection(async_executor, /*host=*/"localhost",
                                std::to_string(server_.ports()[0]),
                                /*is_https=*/false, metric_router_.get());

  ASSERT_TRUE(connection.Init());
  ASSERT_TRUE(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  http_context.request->body = BytesBuffer("request body");
  absl::BlockingCounter counter(1);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_EQ(context.result.status, ExecutionStatus::Success);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      test::WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();

  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_THAT(keys, IsEmpty());

  // Test otel metrics.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  // Request body size.
  const std::map<std::string, std::string> request_body_label_kv = {
      {std::string(kServerAddress), "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap request_body_dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(request_body_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      request_body_metric_point_data = core::GetMetricPointData(
          kClientRequestBodySizeMetric, request_body_dimensions, data);

  ASSERT_TRUE(request_body_metric_point_data.has_value());

  ASSERT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          request_body_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData request_body_histogram_data =
      reinterpret_cast<const opentelemetry::sdk::metrics::HistogramPointData&>(
          request_body_metric_point_data.value());

  auto request_body_histogram_data_max =
      std::get_if<int64_t>(&request_body_histogram_data.max_);
  EXPECT_EQ(*request_body_histogram_data_max, 12);

  // Response body size.
  const std::map<std::string, std::string> response_body_label_kv = {
      {std::string(kServerAddress), "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      response_body_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(response_body_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      response_body_metric_point_data = core::GetMetricPointData(
          kClientResponseBodySizeMetric, response_body_dimensions, data);

  ASSERT_TRUE(response_body_metric_point_data.has_value());

  ASSERT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          response_body_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData response_body_histogram_data =
      reinterpret_cast<const opentelemetry::sdk::metrics::HistogramPointData&>(
          response_body_metric_point_data.value());

  auto response_body_histogram_data_max =
      std::get_if<int64_t>(&response_body_histogram_data.max_);
  EXPECT_EQ(*response_body_histogram_data_max, 0);
}
}  // namespace
}  // namespace google::scp::core
