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

#include "proxy_bridge.h"

#include <functional>
#include <utility>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

#include "logging.h"
#include "socket_types.h"

// We need to use boost::bind instead of std::bind, otherwise
// asio::bind_executors won't work.
using boost::bind;
using boost::asio::bind_cancellation_slot;
using boost::asio::bind_executor;
using boost::asio::const_buffer;
using boost::asio::mutable_buffer;
using boost::system::error_code;
namespace errc = boost::system::errc;
using boost::asio::error::eof;
using std::move;

namespace placeholders = boost::asio::placeholders;

namespace google::scp::proxy {

std::atomic<uint64_t> ProxyBridge::connection_id_counter = 0;

ProxyBridge::~ProxyBridge() {
#ifndef NDEBUG
  LogInfo("[", connection_id_, "]",
          "Destructing connection. UP = ", upstream_size_,
          ", DOWN = ", downstream_size_);
#endif
  error_code ec;
  client_sock_.close(ec);
  dest_sock_.close(ec);
}

void ProxyBridge::PerformSocks5Handshake() {
  // Start the handshake by reading the client first.
  auto buffer = upstream_buff_.ReserveAtLeast<mutable_buffer>(1);
  client_sock_.async_read_some(
      buffer,
      bind_executor(strand_, bind(&ProxyBridge::Socks5HandshakeHandler,
                                  shared_from_this(), placeholders::error,
                                  placeholders::bytes_transferred)));
}

void ProxyBridge::Socks5HandshakeHandler(const error_code& ec,
                                         size_t bytes_read) {
  upstream_buff_.Commit(bytes_read);
  if (ec.failed()) {
    return;
  }

  while (socks5_state_.state() != Socks5State::kSuccess &&
         socks5_state_.Proceed(upstream_buff_)) {}
  // If we need to read more data to proceed, schedule reading.
  if (socks5_state_.InsufficientBuffer(upstream_buff_)) {
    auto buffer = upstream_buff_.ReserveAtLeast<mutable_buffer>(1);
    client_sock_.async_read_some(
        buffer,
        bind_executor(strand_, bind(&ProxyBridge::Socks5HandshakeHandler,
                                    shared_from_this(), placeholders::error,
                                    placeholders::bytes_transferred)));
    return;
  }

  if (socks5_state_.state() == Socks5State::kSuccess) {
    // TODO: implement
  }
}

void ProxyBridge::ForwardTraffic() {
  // Now determine if we need to schedule IO operations.
  if (!reading_client_ && client_readable_ && dest_writable_ &&
      upstream_buff_.data_size() < kMaxBufferSize) {
    auto buffer = upstream_buff_.ReserveAtLeast<mutable_buffer>(kReadSize);
    reading_client_ = true;
    client_sock_.async_read_some(
        buffer,
        bind_executor(strand_, bind(&ProxyBridge::ClientReadHandler,
                                    shared_from_this(), placeholders::error,
                                    placeholders::bytes_transferred)));
  }
  if (!writing_client_ && client_writable_ &&
      downstream_buff_.data_size() > 0u) {
    auto buffer = downstream_buff_.Peek<const_buffer>();
    writing_client_ = true;
    client_sock_.async_write_some(
        buffer,
        bind_executor(strand_, bind(&ProxyBridge::ClientWriteHandler,
                                    shared_from_this(), placeholders::error,
                                    placeholders::bytes_transferred)));
  }
  if (!reading_dest_ && dest_readable_ && client_writable_ &&
      downstream_buff_.data_size() < kMaxBufferSize) {
    auto buffer = downstream_buff_.ReserveAtLeast<mutable_buffer>(kReadSize);
    reading_dest_ = true;
    dest_sock_.async_read_some(
        buffer,
        bind_executor(strand_, bind(&ProxyBridge::DestReadHandler,
                                    shared_from_this(), placeholders::error,
                                    placeholders::bytes_transferred)));
  }
  if (!writing_dest_ && dest_writable_ && upstream_buff_.data_size() > 0u) {
    auto buffer = upstream_buff_.Peek<const_buffer>();
    writing_dest_ = true;
    dest_sock_.async_write_some(
        buffer,
        bind_executor(strand_, bind(&ProxyBridge::DestWriteHandler,
                                    shared_from_this(), placeholders::error,
                                    placeholders::bytes_transferred)));
  }
}

void ProxyBridge::ClientReadHandler(const error_code& ec, size_t bytes_read) {
  reading_client_ = false;
  upstream_buff_.Commit(bytes_read);
  if (ec.failed()) {
    if (ec == eof) {
      LogInfo("[", connection_id_, "]",
              "Client connection successfully closed by peer.");
    } else {
      LogError("[", connection_id_, "]", "Client read failed with error ",
               ec.value());
    }
    client_readable_ = false;
    if (upstream_buff_.data_size() == 0) {
      error_code shutdown_ec;
      dest_sock_.shutdown(Socket::shutdown_send, shutdown_ec);
    }
  }
  ForwardTraffic();
}

void ProxyBridge::ClientWriteHandler(const error_code& ec,
                                     size_t bytes_written) {
  writing_client_ = false;
  downstream_buff_.Drain(bytes_written);
#ifndef NDEBUG
  downstream_size_ += bytes_written;
#endif
  error_code shutdown_ec;
  if (ec.failed()) {
    LogError("[", connection_id_, "]", "Client write failed with error ",
             ec.value());
    client_writable_ = false;
    dest_sock_.shutdown(Socket::shutdown_receive, shutdown_ec);
  }
  if (!dest_readable_ && downstream_buff_.data_size() == 0) {
    client_writable_ = false;
    client_sock_.shutdown(Socket::shutdown_send, shutdown_ec);
  }
  ForwardTraffic();
}

void ProxyBridge::DestReadHandler(const error_code& ec, size_t bytes_read) {
  reading_dest_ = false;
  downstream_buff_.Commit(bytes_read);
  if (ec.failed()) {
    if (ec == eof) {
      LogInfo("[", connection_id_, "]",
              "Dest connection successfully closed by peer.");
    } else {
      LogError("[", connection_id_, "]", "Dest read failed with error ",
               ec.value());
    }
    dest_readable_ = false;
    if (downstream_buff_.data_size() == 0) {
      error_code shutdown_ec;
      client_sock_.shutdown(Socket::shutdown_send, shutdown_ec);
    }
  }
  ForwardTraffic();
}

void ProxyBridge::DestWriteHandler(const error_code& ec, size_t bytes_written) {
  writing_dest_ = false;
  upstream_buff_.Drain(bytes_written);
#ifndef NDEBUG
  upstream_size_ += bytes_written;
#endif
  error_code shutdown_ec;
  if (ec.failed()) {
    LogError("[", connection_id_, "]", "Dest write failed with error ",
             ec.value());
    dest_writable_ = false;
    client_sock_.shutdown(Socket::shutdown_receive, shutdown_ec);
  }
  if (!client_readable_ && upstream_buff_.data_size() == 0) {
    dest_writable_ = false;
    dest_sock_.shutdown(Socket::shutdown_send, shutdown_ec);
  }
  ForwardTraffic();
}

void ProxyBridge::ConnectHandler(const error_code& ec) {
  if (ec.failed()) {
    // TODO: log
    return;
  }
  if (socks5_state_.ConnectionSucceed()) {
    ForwardTraffic();
  }
}

void ProxyBridge::SetSocks5StateCallbacks() {
  socks5_state_.SetConnectCallback([this](const sockaddr* addr, size_t size) {
    Endpoint endpoint(addr, size);
    dest_sock_.async_connect(
        endpoint, bind_executor(strand_, bind(&ProxyBridge::ConnectHandler,
                                              this->shared_from_this(),
                                              placeholders::error)));
    return Socks5State::kStatusInProgress;
  });

  socks5_state_.SetResponseCallback([this](const void* data, size_t len) {
    // The responses are supposed to be tiny and we should expect this succeeds
    // immediately.
    // TODO: make this async as well.
    error_code ec;
    boost::asio::write(client_sock_, const_buffer(data, len), ec);
    if (ec.failed()) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });

  socks5_state_.SetDestAddressCallback(
      [this](sockaddr* addr, size_t* len, bool remote) {
        error_code ec;
        Endpoint ep;
        if (remote) {
          ep = dest_sock_.remote_endpoint(ec);
        } else {
          ep = dest_sock_.local_endpoint(ec);
        }
        if (ec.failed()) {
          return Socks5State::kStatusFail;
        }
        memcpy(addr, ep.data(), ep.size());
        *len = ep.size();
        return Socks5State::kStatusOK;
      });

  // In the bind callback, we basically just wait to accept a new connection. In
  // the meantime, we should wait for client error, in case the client drops
  // connection.
  socks5_state_.SetBindCallback([this](uint16_t& port) {
    auto* acceptor =
        acceptor_pool_->GetAcceptor(client_sock_.get_executor(), port);
    if (acceptor == nullptr) {
      return Socks5State::kStatusFail;
    }
    // Start to async accept a connection from the acceptor, with a
    // per-operation cancellation slot. This way, if the client connection
    // drops, we can cancel this accept operation as well.
    acceptor->async_accept(bind_executor(
        strand_,
        bind_cancellation_slot(
            cancel_signal_.slot(),
            [self = shared_from_this()](const error_code& ec, Socket sock) {
              if (ec.failed()) {
                self->StopWaitingInbound(false /* client error */);
                return;
              }
              self->AcceptInboundConnection(move(sock));
            })));
    // Wait for client error happens. looks like asio cannot wait_error on a
    // proper EOF. So we use wait_read instead. This is probably because the
    // underlying epoll_wait behavior. This operation is cancelled when the
    // async_accept() above completes.
    client_sock_.async_wait(Socket::wait_read,
                            bind_executor(strand_, [self = shared_from_this()](
                                                       const error_code& ec) {
                              if (ec != errc::operation_canceled) {
                                self->StopWaitingInbound();
                              }
                            }));
    return Socks5State::kStatusOK;
  });
}

void ProxyBridge::AcceptInboundConnection(Socket sock) {
  LogInfo("[", connection_id_, "]", "Accepted inbound connection.");
  // Cancel the async_wait on errors
  error_code ec;
  client_sock_.cancel(ec);
  dest_sock_ = move(sock);
  if (!socks5_state_.ConnectionSucceed()) {
    return;
  }
  ForwardTraffic();
}

void ProxyBridge::StopWaitingInbound(bool client_error) {
  if (client_error) {
    cancel_signal_.emit(boost::asio::cancellation_type::total);
  } else {
    error_code ec;
    client_sock_.cancel(ec);
  }
}

}  // namespace google::scp::proxy
