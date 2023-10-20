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

#include <stdint.h>

#include <atomic>
#include <memory>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include "acceptor_pool.h"
#include "buffer.h"
#include "logging.h"
#include "socket_types.h"
#include "socks5_state.h"

namespace google::scp::proxy {
// ProxyBridge is the implementation of traffic forwarding between client and
// destination sockets. Each ProxyBridge object manages one proxy session. This
// class is normally thread safe, as we added asio::strand to ensure
// thread-safety with asio handlers in multi-thread environments.
class ProxyBridge : public std::enable_shared_from_this<ProxyBridge> {
 public:
  static constexpr size_t kMaxBufferSize = 1024 * 1024;
  static constexpr size_t kReadSize = 64 * 1024;

  // Construct a ProxyBridge with a connected client socket. SocketType can be
  // any stream socket implementation of boost::asio.
  template <typename SocketType>
  explicit ProxyBridge(
      SocketType client_sock, AcceptorPool* acceptor_pool = nullptr,
      const std::shared_ptr<Freelist<Buffer::Block>>& freelist = nullptr)
      : connection_id_(connection_id_counter.fetch_add(1)),
        strand_(client_sock.get_executor()),
        client_sock_(std::move(client_sock)),
        dest_sock_(client_sock.get_executor()),
        upstream_buff_(freelist),
        downstream_buff_(freelist),
#ifndef NDEBUG
        upstream_size_(0ul),
        downstream_size_(0ul),
#endif
        acceptor_pool_(acceptor_pool) {
    SetSocks5StateCallbacks();
  }

  // Construct a ProxyBridge with sockets on two sides already open. SocketType
  // can be any stream socket implementation of boost::asio.
  template <typename SocketType1, typename SocketType2>
  ProxyBridge(
      SocketType1 client_sock, SocketType2 dest_sock,
      AcceptorPool* acceptor_pool = nullptr,
      const std::shared_ptr<Freelist<Buffer::Block>>& freelist = nullptr)
      : connection_id_(connection_id_counter.fetch_add(1)),
        strand_(client_sock.get_executor()),
        client_sock_(std::move(client_sock)),
        dest_sock_(std::move(dest_sock)),
        upstream_buff_(freelist),
        downstream_buff_(freelist),
#ifndef NDEBUG
        upstream_size_(0ul),
        downstream_size_(0ul),
#endif
        acceptor_pool_(acceptor_pool) {
    SetSocks5StateCallbacks();
  }

  ~ProxyBridge();

  // Perform socks5 handshake asynchronously.
  void PerformSocks5Handshake();

  // To be called when the events required by the handshake arise.
  void Socks5HandshakeHandler(const boost::system::error_code& ec,
                              size_t bytes_read);

  // To be called when async connect to destination completes.
  void ConnectHandler(const boost::system::error_code& ec);

  // Set the callback hooks for Socks5State.
  void SetSocks5StateCallbacks();

  // Schedule async IO operations for forwarding the traffic.
  void ForwardTraffic();

  // The handler for reading the client socket.
  void ClientReadHandler(const boost::system::error_code& ec,
                         size_t bytes_read);
  // The handler for writing the client socket.
  void ClientWriteHandler(const boost::system::error_code& ec,
                          size_t bytes_written);
  // The handler for reading the dest socket.
  void DestReadHandler(const boost::system::error_code& ec, size_t bytes_read);
  // The handler for writing the dest socket.
  void DestWriteHandler(const boost::system::error_code& ec,
                        size_t bytes_written);

  Executor GetExecutor() { return client_sock_.get_executor(); }

  // Accept an inbound connection if this object was processing a BIND request.
  void AcceptInboundConnection(Socket sock);
  // Stop waiting to accept an inbound connection.
  void StopWaitingInbound(bool client_error = true);

 private:
  static std::atomic<uint64_t> connection_id_counter;
  const uint64_t connection_id_;

  // The "strand" associated to this object. This allows us to reduce locking.
  boost::asio::strand<Socket::executor_type> strand_;

  // The connection from client.
  Socket client_sock_;
  // The connection to destination.
  Socket dest_sock_;
  // The buffer that is read from client and written to the destination.
  Buffer upstream_buff_;
  // The buffer that is read from destination and written to the client.
  Buffer downstream_buff_;
  // The socks5 handshake state.
  Socks5State socks5_state_;
#ifndef NDEBUG  // Record the number of bytes in debug mode.
  size_t upstream_size_;
  size_t downstream_size_;
#endif
  boost::asio::cancellation_signal cancel_signal_;
  AcceptorPool* acceptor_pool_;
  // Flags indicating the state of the proxy connection. We only have 8 of them,
  // so we are using discrete bool instead of a bit field uint64_t. If more
  // flags are needed, we may consider compacting them into a uint32/64_t.
  bool reading_client_ = false;
  bool writing_client_ = false;
  bool client_readable_ = true;
  bool client_writable_ = true;
  bool reading_dest_ = false;
  bool writing_dest_ = false;
  bool dest_readable_ = true;
  bool dest_writable_ = true;
};

}  // namespace google::scp::proxy
