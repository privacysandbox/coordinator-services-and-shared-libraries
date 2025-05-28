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

#include <array>
#include <csignal>
#include <memory>
#include <vector>

#include <nghttp2/asio_http2_server.h>

#include "absl/synchronization/blocking_counter.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/http2_client/mock/mock_http_connection.h"
#include "cc/core/http2_client/src/error_codes.h"
#include "cc/core/http2_client/src/http_client_def.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/telemetry/mock/in_memory_metric_router.h"
#include "cc/core/telemetry/src/common/metric_utils.h"
#include "cc/core/test/utils/conditional_wait.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

namespace privacy_sandbox::pbs_common {
namespace {
using ::opentelemetry::sdk::resource::SemanticConventions::kServerAddress;
using ::testing::IsEmpty;

constexpr std::array<double, 78> kLatencyHistogramBoundaries = {
    0, 0.0001, 0.00012589254117941674,
    0.00015848931924611136, 0.000199526231496888, 0.00025118864315095806,
    0.00031622776601683805, 0.0003981071705534974, 0.0005011872336272725,
    0.0006309573444801935, 0.000794328234724282, 0.0010000000000000007,
    0.0012589254117941681, 0.0015848931924611145, 0.001995262314968881,
    0.002511886431509582, 0.0031622776601683824, 0.003981071705534976,
    0.005011872336272727, 0.00630957344480194, 0.007943282347242824,
    0.010000000000000012, 0.012589254117941687, 0.015848931924611155,
    0.019952623149688823, 0.025118864315095836, 0.03162277660168384,
    0.03981071705534978, 0.050118723362727303, 0.06309573444801943,
    0.07943282347242828, 0.10000000000000017, 0.12589254117941695,
    0.15848931924611165, 0.19952623149688836, 0.2511886431509585,
    0.31622776601683855, 0.3981071705534981, 0.5011872336272734,
    0.6309573444801946, 0.7943282347242833, 1.0000000000000022,
    1.2589254117941702, 1.5848931924611172, 1.9952623149688844,
    2.5118864315095863, 3.1622776601683875, 3.981071705534983,
    5.011872336272736, 6.30957344480195, 7.943282347242837,
    10.000000000000028, 12.589254117941708, 15.848931924611183,
    19.952623149688858, 25.11886431509588, 31.622776601683892,
    39.810717055349855, 50.11872336272739, 63.095734448019535,
    79.43282347242842, 100.00000000000034, 125.89254117941718,
    158.4893192461119, 199.5262314968887, 251.18864315095894,
    316.22776601683915, 398.10717055349875, 501.18723362727417,
    630.9573444801956, 794.3282347242847, 1000.000000000004,
    1258.9254117941723, 1584.89319246112, 1995.2623149688877,
    2511.8864315095907, 3162.2776601683927, 3981.0717055349896,
};

class HttpConnectionTest : public testing::Test {
 protected:
  void SetUp() override {
    server_.num_threads(1);

    boost::system::error_code ec;

    server_.listen_and_serve(ec, /*address=*/"localhost",
                             /*port=*/"0", /*asynchronous=*/true);
    ASSERT_FALSE(ec) << "Failed to start the server: " << ec.message();

    metric_router_ = std::make_unique<InMemoryMetricRouter>();
  }

  void TearDown() override {
    server_.stop();
    server_.join();
  }

  nghttp2::asio_http2::server::http2 server_;
  std::unique_ptr<InMemoryMetricRouter> metric_router_;
};

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
          EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(
                                          SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
          counter.DecrementCount();
        }
      };

  ExecutionResult execution_result =
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
          EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(
                                          SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
          counter.DecrementCount();
        }
      };

  ExecutionResult execution_result =
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
                  SC_HTTP2_CLIENT_HTTP_METHOD_NOT_SUPPORTED);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
                  SC_HTTP2_CLIENT_FAILED_TO_ISSUE_HTTP_REQUEST);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();

  counter.Wait();
  execution_result = WaitUntilOrReturn([&]() { return !connection.IsReady(); });
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
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
      {"server.address", "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_server_latency_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_server_latency_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_server_latency_metric_point_data = GetMetricPointData(
          "http.client.server_latency", client_server_latency_dimensions, data);

  ASSERT_TRUE(client_server_latency_metric_point_data.has_value());

  ASSERT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          client_server_latency_metric_point_data.value()));

  auto metric_point_data =
      std::get<opentelemetry::sdk::metrics::HistogramPointData>(
          client_server_latency_metric_point_data.value());

  const std::vector<double>& boundaries = metric_point_data.boundaries_;

  // Check if the boundaries vector matches the expected boundaries.
  ASSERT_EQ(boundaries.size(), kLatencyHistogramBoundaries.size())
      << "Boundaries vector size mismatch.";

  for (size_t i = 0; i < boundaries.size(); ++i) {
    EXPECT_DOUBLE_EQ(boundaries[i], kLatencyHistogramBoundaries[i])
        << "Mismatch at index " << i;
  }
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
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
      {"server.address", "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_request_duration_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_request_duration_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_request_duration_metric_point_data =
          GetMetricPointData("http.client.request.duration",
                             client_request_duration_dimensions, data);

  ASSERT_TRUE(client_request_duration_metric_point_data.has_value());

  ASSERT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          client_request_duration_metric_point_data.value()));

  auto metric_point_data =
      std::get<opentelemetry::sdk::metrics::HistogramPointData>(
          client_request_duration_metric_point_data.value());

  const std::vector<double>& boundaries = metric_point_data.boundaries_;

  // Check if the boundaries vector matches the expected boundaries.
  ASSERT_EQ(boundaries.size(), kLatencyHistogramBoundaries.size())
      << "Boundaries vector size mismatch.";

  for (size_t i = 0; i < boundaries.size(); ++i) {
    EXPECT_DOUBLE_EQ(boundaries[i], kLatencyHistogramBoundaries[i])
        << "Mismatch at index " << i;
  }
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
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
      {{"server.address", "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_connection_duration_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_connection_duration_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_connection_duration_metric_point_data =
          GetMetricPointData("http.client.connection.duration",
                             client_connection_duration_dimensions, data);

  ASSERT_TRUE(client_connection_duration_metric_point_data.has_value());

  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          client_connection_duration_metric_point_data.value()));

  auto metric_point_data =
      std::get<opentelemetry::sdk::metrics::HistogramPointData>(
          client_connection_duration_metric_point_data.value());

  const std::vector<double>& boundaries = metric_point_data.boundaries_;

  // Check if the boundaries vector matches the expected boundaries.
  ASSERT_EQ(boundaries.size(), kLatencyHistogramBoundaries.size())
      << "Boundaries vector size mismatch.";

  for (size_t i = 0; i < boundaries.size(); ++i) {
    EXPECT_DOUBLE_EQ(boundaries[i], kLatencyHistogramBoundaries[i])
        << "Mismatch at index " << i;
  }
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
                  SC_HTTP2_CLIENT_FAILED_TO_ISSUE_HTTP_REQUEST);
        counter.DecrementCount();
      };

  ExecutionResult execution_result =
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
  ASSERT_SUCCESS(execution_result)
      << "Connection is not ready within the expected time.";
  execution_result = connection.Execute(http_context);
  EXPECT_SUCCESS(execution_result);

  release_response.DecrementCount();
  counter.Wait();

  execution_result = WaitUntilOrReturn([&]() { return !connection.IsReady(); });
  ASSERT_SUCCESS(execution_result) << "Connection has not be dropped.";

  // Test otel metrics.
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  // Connection Duration
  const std::map<std::string, std::string> client_connection_duration_label_kv =
      {{"server.address", "localhost"}};

  const opentelemetry::sdk::common::OrderedAttributeMap
      client_connection_duration_dimensions(
          (opentelemetry::common::KeyValueIterableView<
              std::map<std::string, std::string>>(
              client_connection_duration_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      client_connection_duration_metric_point_data =
          GetMetricPointData("http.client.connection.duration",
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
      client_connect_error_metric_point_data = GetMetricPointData(
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
      WaitUntilOrReturn([&]() { return connection.IsReady(); });
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
      request_body_metric_point_data = GetMetricPointData(
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
      response_body_metric_point_data = GetMetricPointData(
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
}  // namespace privacy_sandbox::pbs_common
