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

#include "core/http2_forwarder/src/http2_forwarder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <nghttp2/asio_http2_server.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "absl/strings/str_cat.h"
#include "cc/core/http2_client/src/http2_client.h"
#include "core/async_executor/src/async_executor.h"
#include "core/http2_client/src/http2_client.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using namespace nghttp2::asio_http2;          // NOLINT
using namespace nghttp2::asio_http2::server;  // NOLINT
using namespace std::chrono_literals;         // NOLINT
using namespace std::placeholders;            // NOLINT

using google::scp::core::test::WaitUntil;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Contains;
using testing::Pair;

namespace google::scp::core::test {

namespace {
class TestHttp2Server {
 public:
  TestHttp2Server(string address, string port, size_t num_threads)
      : address_(address), port_(port), num_threads_(num_threads) {}

  ~TestHttp2Server() { server.join(); }

  void Run() {
    boost::system::error_code ec;

    server.num_threads(num_threads_);

    server.handle("/ping", [&](const request& req, const response& res) {
      req.on_data([&](const uint8_t* data, std::size_t length) {
        validate_request_data_hook_(data, length);
      });
      validate_request_hook_(req);
      write_response_hook_(res);
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
  string address_;
  string port_;
  std::atomic<bool> is_running_{false};
  size_t num_threads_;
  std::function<void(const request&)> validate_request_hook_;
  std::function<void(const uint8_t* data, std::size_t length)>
      validate_request_data_hook_;
  std::function<void(const response&)> write_response_hook_;
};
}  // namespace

class Http2ForwarderTest : public testing::Test {
 protected:
  Http2ForwarderTest() {
    async_executor_ = make_shared<AsyncExecutor>(2, 100);
    http2_client_ = make_shared<HttpClient>(async_executor_);
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(http2_client_->Init());
    EXPECT_SUCCESS(http2_client_->Run());
    http2_server_ =
        make_unique<TestHttp2Server>("localhost", "0", 1 /* num threads */);
    http2_server_->Run();
  }

  ~Http2ForwarderTest() {
    EXPECT_SUCCESS(http2_client_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
    http2_server_->Stop();
  }

  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<HttpClientInterface> http2_client_;
  unique_ptr<TestHttp2Server> http2_server_;
};

TEST_F(Http2ForwarderTest,
       TestForwardRequestToRemoteEndpointSuccessfulResponse) {
  http2_server_->validate_request_hook_ = [](const request& request) {
    auto it = request.header().find("request-header-key");
    EXPECT_NE(it, request.header().end());
    EXPECT_EQ(it->second.value, "request-header-value");
  };
  http2_server_->validate_request_data_hook_ = [](const uint8_t* data,
                                                  std::size_t length) {
    // This method is called twice, once with data, and the other time to
    // indicate EOF with length==0;
    if (length == 0) {
      return;
    }
    EXPECT_EQ(string((const char*)data, length), "This is data!!!!!");
  };
  http2_server_->write_response_hook_ = [](const response& res) {
    res.write_head(200, {{"response-header-key", {"response-header-value"}}});
    res.end("This is response data!!!\n");
  };

  Http2Forwarder forwarder(http2_client_);

  std::atomic<bool> completed(false);
  AsyncContext<HttpRequest, HttpResponse> http_context(
      make_shared<HttpRequest>(),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->body.ToString(),
                  "This is response data!!!\n");
        EXPECT_THAT(
            *context.response->headers,
            Contains(Pair("response-header-key", "response-header-value")));
        completed = true;
      });
  http_context.request->method = HttpMethod::GET;
  http_context.request->path = make_shared<string>(
      absl::StrCat("http://localhost:", http2_server_->PortInUse(), "/ping"));
  http_context.request->body = BytesBuffer("This is data!!!!!");
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {"request-header-key", "request-header-value"});

  forwarder.RouteRequest(http_context);

  WaitUntil([&]() { return completed.load(); });
}

TEST_F(Http2ForwarderTest,
       TestForwardRequestToRemoteEndpointUnsuccessfulResponse) {
  // No ops for the below methods as these are validated elsewhere in the test
  // suite.
  http2_server_->validate_request_hook_ = [](const request& request) {};
  http2_server_->validate_request_data_hook_ = [](const uint8_t* data,
                                                  std::size_t length) {};
  http2_server_->write_response_hook_ = [](const response& res) {
    // Induce an error back to the forwarder.
    res.write_head(404);
    res.end();
  };

  Http2Forwarder forwarder(http2_client_);

  std::atomic<bool> completed(false);
  AsyncContext<HttpRequest, HttpResponse> http_context(
      make_shared<HttpRequest>(),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND)));
        completed = true;
      });
  http_context.request->method = HttpMethod::GET;
  http_context.request->path = make_shared<string>(
      absl::StrCat("http://localhost:", http2_server_->PortInUse(), "/ping"));
  http_context.request->body = BytesBuffer("This is data!!!!!");
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {"request-header-key", "request-header-value"});

  forwarder.RouteRequest(http_context);

  WaitUntil([&]() { return completed.load(); });
}

};  // namespace google::scp::core::test
