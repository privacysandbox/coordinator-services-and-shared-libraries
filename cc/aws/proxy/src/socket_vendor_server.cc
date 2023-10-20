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

#include "socket_vendor_server.h"

#include <errno.h>
#include <unistd.h>

#include <memory>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include "logging.h"
#include "socket_types.h"

namespace asio = boost::asio;
using boost::system::error_code;
using std::make_shared;
using std::move;
using std::thread;

namespace google::scp::proxy {
bool SocketVendorServer::Init() {
  if (sock_path_.empty()) {
    LogError("Sock path is empty.");
    return false;
  }
  // Delete the file first.
  if (unlink(sock_path_.c_str()) != 0 && errno != ENOENT) {
    LogError("Cannot remove file: ", sock_path_);
    return false;
  }
  Protocol protocol(AF_UNIX, 0);
  error_code ec;
  acceptor_.open(protocol, ec);
  if (ec.failed()) {
    LogError("Cannot open acceptor: ", ec.message());
    return false;
  }
  acceptor_.set_option(Socket::reuse_address(true), ec);
  if (ec.failed()) {
    LogError("Cannot set option REUSEADDR, ", ec.message());
    return false;
  }
  Endpoint ep{asio::local::stream_protocol::endpoint(sock_path_)};
  acceptor_.bind(ep, ec);
  if (ec.failed()) {
    LogError("Cannot bind on path: ", sock_path_, ", ", ec.message());
    return false;
  }
  acceptor_.listen();
  StartAsyncAccept();
  return true;
}

void SocketVendorServer::Run() {
  if (concurrency_ == 0) {
    concurrency_ = std::thread::hardware_concurrency();
  }
  for (size_t i = 0; i < concurrency_; ++i) {
    workers_.emplace_back([this]() { io_context_.run(); });
  }
  for (auto& w : workers_) {
    w.join();
  }
}

void SocketVendorServer::Stop() {
  io_context_.stop();
}

void SocketVendorServer::StartAsyncAccept() {
  acceptor_.async_accept([this](boost::system::error_code ec, Socket socket) {
    StartAsyncAccept();
    if (!ec) {
      auto pool = make_shared<ClientSessionPool>(move(socket), proxy_endpoint_);
      if (!pool->Start()) {
        pool->Stop();
      }
    }
  });
}

}  // namespace google::scp::proxy
