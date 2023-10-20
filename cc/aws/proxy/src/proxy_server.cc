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

#include "proxy_server.h"

#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/vm_sockets.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio.hpp>

#include "logging.h"
#include "proxy_bridge.h"
#include "socket_types.h"

using std::make_shared;
using std::shared_lock;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::unique_lock;
using std::vector;

using namespace boost::asio;  // NOLINT

namespace google::scp::proxy {

ProxyServer::ProxyServer(const Config& config)
    : acceptor_(io_context_),
      port_(config.socks5_port_),
      vsock_(config.vsock_) {}

void ProxyServer::BindListen() {
  if (vsock_) {
    Protocol protocol(AF_VSOCK, 0);
    acceptor_.open(protocol);
    socket_base::reuse_address reuse_addr(true);
    acceptor_.set_option(reuse_addr);
    sockaddr_vm addr;
    memset(&addr, 0, sizeof(addr));
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = VMADDR_CID_ANY;
    addr.svm_port = port_;
    Endpoint endpoint(&addr, sizeof(addr));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    if (port_ == 0) {
      auto ep = acceptor_.local_endpoint();
      sockaddr_vm* addr = reinterpret_cast<sockaddr_vm*>(ep.data());
      port_ = static_cast<decltype(port_)>(addr->svm_port);
    }
  } else {
    Protocol protocol(AF_INET6, 0);
    acceptor_.open(protocol);
    socket_base::reuse_address reuse_addr(true);
    acceptor_.set_option(reuse_addr);
    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = IN6ADDR_ANY_INIT;
    addr.sin6_port = htons(port_);
    Endpoint endpoint(&addr, sizeof(addr));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    if (port_ == 0) {
      auto ep = acceptor_.local_endpoint();
      sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(ep.data());
      port_ = ntohs(addr->sin6_port);
    }
  }
}

void ProxyServer::StartAsyncAccept() {
  acceptor_.async_accept([this](boost::system::error_code ec, Socket socket) {
    StartAsyncAccept();
    if (!ec) {
      auto bridge =
          make_shared<ProxyBridge>(std::move(socket), &acceptor_pool_);
      bridge->PerformSocks5Handshake();
    }
  });
}

void ProxyServer::Stop() {
  io_context_.stop();
}

void ProxyServer::Run(size_t concurrency) {
  if (concurrency == 0) {
    concurrency = std::thread::hardware_concurrency();
  }
  StartAsyncAccept();
  vector<thread> threads;
  threads.reserve(concurrency);
  for (auto i = 0u; i <= concurrency; ++i) {
    threads.emplace_back([this]() { io_context_.run(); });
    auto& t = threads[i];
    string name = string("worker_") + to_string(i);
    pthread_setname_np(t.native_handle(), name.c_str());
  }
  for (auto i = 0u; i <= concurrency; ++i) {
    threads[i].join();
  }
}

}  // namespace google::scp::proxy
