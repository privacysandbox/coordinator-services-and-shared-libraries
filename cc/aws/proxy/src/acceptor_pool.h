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

#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include <boost/asio/ip/tcp.hpp>

#include "logging.h"
#include "socket_types.h"

namespace google::scp::proxy {
class AcceptorPool {
 public:
  Acceptor* GetAcceptor(const Acceptor::executor_type& executor,
                        uint16_t& port) {
    std::shared_lock rlock(mutex_);
    auto it = pool_.find(port);
    // Most likely, the port exists and we just push the session into the pool.
    if (it != pool_.end()) {
      return &it->second;
    }

    // Otherwise, this is the first time we bind to this port. Allocate a new
    // pool and kickstart it. We need to release the read lock and acquire a
    // write lock to do so.
    rlock.unlock();
    {
      std::unique_lock wlock(mutex_);
      if (port == 0) {  // Bind an arbitrary port
        Acceptor acceptor(executor, Protocol(tcp::v6()));
        uint16_t bound_port = BindListen(acceptor, port);
        auto res = pool_.try_emplace(bound_port, std::move(acceptor));
        port = bound_port;
        return &res.first->second;
      }
      // Bind a specified port
      auto res = pool_.try_emplace(port, executor, Protocol(tcp::v6()));
      auto& acceptor = res.first->second;
      if (res.second) {  // If insertion succeeded
        uint16_t bound_port = BindListen(acceptor, port);
        if (bound_port == 0) {
          pool_.erase(res.first);
          return nullptr;
        }
      }
      return &acceptor;
    }
  }

 private:
  uint16_t BindListen(Acceptor& acceptor, uint16_t port) {
    Endpoint ep(tcp::endpoint(tcp::v6(), port));
    boost::system::error_code ec;
    acceptor.bind(ep, ec);
    if (ec.failed()) {
      LogError("Cannot bind on port ", port, ec.message());
      return 0;
    }
    ep = acceptor.local_endpoint(ec);
    if (ec.failed()) {
      LogError("Cannot get local bound endpoint, ", ec.message());
      return 0;
    }
    auto* addr = reinterpret_cast<sockaddr_in6*>(ep.data());
    port = ntohs(addr->sin6_port);
    acceptor.listen(Socket::max_listen_connections, ec);
    if (ec.failed()) {
      LogError("Cannot listen on port, ", port, ", ", ec.message());
      return 0;
    }
    return port;
  }

  using tcp = boost::asio::ip::tcp;
  std::map<uint16_t, Acceptor> pool_;
  std::shared_mutex mutex_;
};
}  // namespace google::scp::proxy
