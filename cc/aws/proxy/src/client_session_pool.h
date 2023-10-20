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

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

#include "socket_types.h"

namespace google::scp::proxy {

// ClientSessionPool is used by socket_vendor to manage a pool of connections to
// the proxy, for inbound traffic support.
class ClientSessionPool
    : public std::enable_shared_from_this<ClientSessionPool> {
 public:
  // Construct a ClientSessionPool to serve a socket vendor client connection.
  template <typename SocketType>
  ClientSessionPool(SocketType client_sock, const Endpoint& proxy_endpoint)
      : client_sock_(std::move(client_sock)),
        proxy_endpoint_(proxy_endpoint),
        client_strand_(client_sock_.get_executor()) {}

  ~ClientSessionPool();

  bool Start();
  void Stop();

 private:
  // The size of the 2nd response of a BIND request in socks5 protocol, when the
  // response contains IPv6 address.
  static constexpr size_t k2ndBindResponseSize = 22u;
  // The size of the micro buffers, should be >= k2ndBindResponseSize and sizes
  // of any message in socket_vendor_protocol.h
  static constexpr size_t kMicroBufferSize = 24u;
  // The total size of a greeting (3 bytes), and bind request (10 bytes).
  static constexpr size_t kBindRequestSize = 13u;

  // The max number of standby connections in a pool, regardless of the backlog
  // size from listen() call.
  static constexpr int kMaxConnections = 128;

  struct MicroBuffer {
    uint8_t data[kMicroBufferSize];
  };

  // Handle async connect of socket at index.
  void HandleConnect(size_t index, const boost::system::error_code& ec);
  // Handle the 1st response from proxy for socket at index.
  void Handle1stResp(size_t index, size_t bytes_read,
                     const boost::system::error_code& ec);
  // Handle the 2nd response from proxy for socket at index.
  void Handle2ndResp(size_t index, size_t bytes_read,
                     const boost::system::error_code& ec);
  // Handle the client sock error.
  void HandleClientError(const boost::system::error_code& ec);
  // Handle sending fd to the client.
  void HandleSendFd(size_t index, const boost::system::error_code& ec);

  // Initiate or re-initiate the socket at index.
  void InitiateSocket(size_t index);

  Socket client_sock_;
  Endpoint proxy_endpoint_;
  // Strand to serialize IO operations on client_sock_.
  boost::asio::strand<Socket::executor_type> client_strand_;
  // The pool of sockets awaiting inbound connection.
  std::vector<Socket> pool_;
  std::vector<MicroBuffer> buffers_;
  uint8_t proxy_bind_request_[kBindRequestSize];
  std::atomic<size_t> number_of_errors_{0u};
  size_t pool_size_;
  uint16_t port_ = 0u;
  std::atomic<bool> stop_{false};
};

}  // namespace google::scp::proxy
