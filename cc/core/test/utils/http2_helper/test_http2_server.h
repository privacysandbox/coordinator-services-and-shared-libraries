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
#pragma once

#include <gtest/gtest.h>

#include <string>

#include <nghttp2/asio_http2_server.h>

#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::test {
class TestHttp2Server {
 public:
  using Request = nghttp2::asio_http2::server::request;
  using Response = nghttp2::asio_http2::server::response;
  using RequestCallback = nghttp2::asio_http2::server::request_cb;

  explicit TestHttp2Server(
      const std::string& host = "localhost",
      const std::string& port = "0" /* pick any available */,
      int num_threads = 2)
      : host_(host), port_(port), num_threads_(num_threads) {
    server_.num_threads(num_threads_);
  }

  ~TestHttp2Server() { Stop(); }

  int PortNumber() {
    if (is_running_) {
      // Get the first port.
      return server_.ports()[0];
    }
    return -1;
  }

  const std::string& HostName() { return host_; }

  void Handle(std::string path, const RequestCallback& callback) {
    server_.handle(path, callback);
  }

  void Run() {
    boost::system::error_code ec;
    server_.listen_and_serve(ec, host_, port_, true);
    EXPECT_FALSE(ec.failed()) << "Actual error code: " << ec;
    is_running_ = true;
  }

  void Stop() {
    if (!is_running_) {
      return;
    }
    is_running_ = false;
    server_.stop();
    for (auto& io_service : server_.io_services()) {
      io_service->stop();
    }
    server_.join();
  }

 protected:
  std::atomic<bool> is_running_{false};
  nghttp2::asio_http2::server::http2 server_;
  std::string host_;
  std::string port_;
  int num_threads_;
};
}  // namespace google::scp::core::test
