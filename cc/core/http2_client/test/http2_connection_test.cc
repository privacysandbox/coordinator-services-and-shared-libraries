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

#include <gtest/gtest.h>

#include <stdint.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <nghttp2/asio_http2_server.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "cc/core/http2_client/src/http2_client.h"
#include "cc/core/interface/async_context.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/http2_client/mock/mock_http_connection.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using namespace nghttp2::asio_http2;          // NOLINT
using namespace nghttp2::asio_http2::server;  // NOLINT
using namespace std::chrono_literals;         // NOLINT
using namespace std::placeholders;            // NOLINT

using google::scp::core::AsyncExecutor;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Uuid;
using google::scp::core::http2_client::mock::MockHttpConnection;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::bind;
using std::future;
using std::make_shared;
using std::promise;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::vector;
using std::chrono::milliseconds;

namespace google::scp::core {

TEST(HttpConnectionTest, SimpleRequest) {
  http2 server;
  boost::system::error_code ec;
  atomic<bool> release_response = false;
  atomic<bool> response_released = false;
  server.num_threads(1);
  server.handle("/test", [&](const request& req, const response& res) {
    while (!release_response.load()) {
      usleep(10000);
    }

    res.write_head(200);
    res.end();

    response_released = true;
  });
  server.listen_and_serve(ec, "localhost", "0", true);

  auto async_executor = make_shared<AsyncExecutor>(2, 20);
  MockHttpConnection connection(async_executor, "localhost",
                                to_string(server.ports()[0]), false);

  EXPECT_SUCCESS(connection.Init());
  EXPECT_SUCCESS(connection.Run());

  vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->path = make_shared<string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  atomic<bool> is_called(false);
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        is_called.store(true);
      };

  ExecutionResult execution_result = RetryExecutionResult(123);
  while (execution_result.status == ExecutionStatus::Retry) {
    execution_result = connection.Execute(http_context);
    usleep(1000);
  }

  EXPECT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  release_response = true;
  while (!response_released.load()) {
    usleep(1000);
  }

  WaitUntil([&]() { return is_called.load(); });
  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  server.stop();
  server.join();
}

TEST(HttpConnectionTest, CancelCallbacks) {
  http2 server;
  boost::system::error_code ec;
  atomic<bool> release_response = false;
  server.num_threads(1);
  server.handle("/test", [&](const request& req, const response& res) {
    while (!release_response.load()) {
      usleep(10000);
    }

    res.write_head(200);
    res.end();
  });
  server.listen_and_serve(ec, "localhost", "0", true);

  auto async_executor = make_shared<AsyncExecutor>(2, 20);
  MockHttpConnection connection(async_executor, "localhost",
                                to_string(server.ports()[0]), false);

  EXPECT_SUCCESS(connection.Init());
  EXPECT_SUCCESS(connection.Run());

  vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->path = make_shared<string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  bool is_called = false;
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (!is_called) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
        }
      };

  ExecutionResult execution_result = RetryExecutionResult(123);
  while (execution_result.status == ExecutionStatus::Retry) {
    execution_result = connection.Execute(http_context);
    usleep(1000);
  }

  EXPECT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  connection.CancelPendingCallbacks();
  EXPECT_EQ(is_called, true);

  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  release_response = true;
  connection.Stop();
  server.stop();
  server.join();
}

TEST(HttpConnectionTest, StopRemovesCallback) {
  http2 server;
  boost::system::error_code ec;
  atomic<bool> release_response = false;
  server.num_threads(1);
  server.handle("/test", [&](const request& req, const response& res) {
    while (!release_response.load()) {
      usleep(10000);
    }

    res.write_head(200);
    res.end();
  });
  server.listen_and_serve(ec, "localhost", "0", true);

  auto async_executor = make_shared<AsyncExecutor>(2, 20);
  MockHttpConnection connection(async_executor, "localhost",
                                to_string(server.ports()[0]), false);

  EXPECT_SUCCESS(connection.Init());
  EXPECT_SUCCESS(connection.Run());

  vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();
  http_context.request->path = make_shared<string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  bool is_called = false;
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (!is_called) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
        }
      };

  ExecutionResult execution_result = RetryExecutionResult(123);
  while (execution_result.status == ExecutionStatus::Retry) {
    execution_result = connection.Execute(http_context);
    usleep(1000);
  }

  EXPECT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);
  EXPECT_EQ(is_called, true);

  release_response = true;
  connection.Stop();
  server.stop();
  server.join();
}

}  // namespace google::scp::core
