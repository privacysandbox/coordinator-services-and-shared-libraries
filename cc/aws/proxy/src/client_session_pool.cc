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

#include "client_session_pool.h"

#include <utility>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

#include "logging.h"
#include "socket_vendor_protocol.h"

using boost::bind;
using boost::asio::async_read;
using boost::asio::async_write;
using boost::asio::bind_executor;
using boost::asio::const_buffer;
using boost::asio::mutable_buffer;
using boost::system::error_code;
using std::move;
using std::vector;

namespace placeholders = boost::asio::placeholders;
namespace asio = boost::asio;

namespace google::scp::proxy {

ClientSessionPool::~ClientSessionPool() {
  LogInfo("socket_vendor: application closure on bound port ", port_);
}

bool ClientSessionPool::Start() {
  pool_.clear();
  // This just initiates the pool. Most of the calls below are blocking, but it
  // is fine since we only do it once at the beginning.
  error_code ec;
  socket_vendor::BindRequest bind_req;
  asio::read(client_sock_, mutable_buffer(&bind_req, sizeof(bind_req)), ec);
  if (ec.failed() ||
      bind_req.type != socket_vendor::MessageType::kBindRequest) {
    LogError("socket_vendor: bad BindRequest, ", ec.message());
    return false;
  }
  LogInfo("socket_vendor: bindRequest on port ", bind_req.port);
  // On the bind call, we initiate the first connection to proxy to see if
  // binding on port can be successfully made.
  Socket first_socket(client_sock_.get_executor());
  first_socket.connect(proxy_endpoint_, ec);
  if (ec.failed()) {
    LogError("socket_vendor: cannot connect to proxy, ", ec.message());
    return false;
  }

  uint16_t port = htons(bind_req.port);
  static constexpr uint8_t bind_req_template[] = {
      0x05, 0x01, 0x00,        // <- Greeting
      0x05, 0x02, 0x00, 0x01,  // <- request header
      0x00, 0x00, 0x00, 0x00,  // <- addr = 0.0.0.0
      0x00, 0x00};             // <- port, to be assigned
  memcpy(proxy_bind_request_, bind_req_template, sizeof(proxy_bind_request_));
  // Fill in the port
  memcpy(proxy_bind_request_ + sizeof(proxy_bind_request_) - 2, &port, 2);
  asio::write(first_socket,
              mutable_buffer(proxy_bind_request_, sizeof(proxy_bind_request_)),
              ec);
  if (ec.failed()) {
    LogError("socket_vendor: cannot write to proxy, ", ec.message());
    return false;
  }

  // Two messages has been sent. Receive replies now. Method selection reply:
  //     +----+--------+
  //     |VER | METHOD |
  //     +----+--------+
  //     | 1  |   1    |
  //     +----+--------+
  // Bind request reply:
  //     +----+-----+-------+------+----------+----------+
  //     |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  //     +----+-----+-------+------+----------+----------+
  //     | 1  |  1  | X'00' |  1   | Variable |    2     |
  //     +----+-----+-------+------+----------+----------+
  // See socks5_state.cc, the Bind request reply currently is 10 bytes:
  // uint8_t bind_resp[] = {0x05, 0x00, 0x00, 0x01, 0x00,
  //                        0x00, 0x00, 0x00, 0x00, 0x00};
  // So we are supposed to receive a 12-byte fixed-size response.
  uint8_t proxy_resp_buf[12];
  asio::read(first_socket,
             mutable_buffer(proxy_resp_buf, sizeof(proxy_resp_buf)), ec);
  if (ec.failed() || proxy_resp_buf[3] != 0x00) {
    LogError("socket_vendor: cannot read response from proxy, ", ec.message());
    return false;
  }
  // Get the port bound returned by the proxy server.
  memcpy(&port, proxy_resp_buf + sizeof(proxy_resp_buf) - 2, 2);
  // Put into the request again for the pool to use.
  memcpy(proxy_bind_request_ + sizeof(proxy_bind_request_) - 2, &port, 2);
  port = ntohs(port);
  port_ = port;

  // Now send the bind response to client.
  socket_vendor::BindResponse bind_resp;
  bind_resp.port = port;
  asio::write(client_sock_, const_buffer(&bind_resp, sizeof(bind_resp)), ec);
  if (ec.failed()) {
    LogError("socket_vendor: cannot send response to client, ", ec.message());
    return false;
  }

  // Here we are done with bind() call. Preserve the first socket.
  pool_.push_back(move(first_socket));

  // Handle the listen call.
  socket_vendor::ListenRequest listen_req;
  asio::read(client_sock_, mutable_buffer(&listen_req, sizeof(listen_req)), ec);
  if (ec.failed() ||
      listen_req.type != socket_vendor::MessageType::kListenRequest) {
    return false;
  }
  socket_vendor::ListenResponse listen_resp;
  asio::write(client_sock_, mutable_buffer(&listen_resp, sizeof(listen_resp)),
              ec);
  if (ec.failed()) {
    return false;
  }
  int32_t backlog = listen_req.backlog;
  if (backlog < 0) {
    return false;
  }
  LogInfo("socket_vendor: application bind-listen port ", port);
  LogInfo("socket_vendor: ListenRequest of backlog = ", backlog);
  backlog = backlog > kMaxConnections ? kMaxConnections : backlog;
  pool_size_ = static_cast<size_t>(backlog);
  LogInfo("socket_vendor: hydrating pool of size ", pool_size_);

  pool_.reserve(pool_size_);
  buffers_.resize(pool_size_);

  // Now we are done! Monitor the client sock for errors:
  client_sock_.async_wait(Socket::wait_error,
                          bind(&ClientSessionPool::HandleClientError,
                               shared_from_this(), placeholders::error));

  // Now hydrate the pool. pool_[0] is special, as we already ran the connect
  // and first response phases.
  pool_[0].async_read_some(
      mutable_buffer(buffers_[0].data, k2ndBindResponseSize),
      bind(&ClientSessionPool::Handle2ndResp, shared_from_this(), 0u,
           placeholders::bytes_transferred, placeholders::error));

  // Hydrate the rest of the pool, serially, to avoid overloading the proxy.
  pool_.emplace_back(client_sock_.get_executor());
  pool_[1].async_connect(proxy_endpoint_,
                         bind(&ClientSessionPool::HandleConnect,
                              shared_from_this(), 1, placeholders::error));
  return true;
}

void ClientSessionPool::InitiateSocket(size_t index) {
  auto& sock = pool_[index];
  error_code err_ignore;
  sock.close(err_ignore);
  // Re-create a new socket
  sock = Socket(client_sock_.get_executor());
  sock.async_connect(proxy_endpoint_,
                     bind(&ClientSessionPool::HandleConnect, shared_from_this(),
                          index, placeholders::error));
}

void ClientSessionPool::HandleConnect(size_t index, const error_code& ec) {
  auto& sock = pool_[index];
  if (stop_.load()) {
    return;
  }
  if (ec.failed()) {
    number_of_errors_++;
    InitiateSocket(index);
    return;
  }
  error_code write_error;
  // The buffer is tiny, the socket is idle, so no point doing async write here.
  asio::write(sock,
              const_buffer(proxy_bind_request_, sizeof(proxy_bind_request_)),
              write_error);
  if (write_error.failed()) {
    number_of_errors_++;
    InitiateSocket(index);
    return;
  }
  // Then we wait for the first response.
  async_read(sock, mutable_buffer(buffers_[index].data, 12),
             bind(&ClientSessionPool::Handle1stResp, shared_from_this(), index,
                  placeholders::bytes_transferred, placeholders::error));
}

void ClientSessionPool::Handle1stResp(size_t index, size_t bytes_read,
                                      const error_code& ec) {
  auto& sock = pool_[index];
  auto& buf = buffers_[index].data;
  if (stop_.load()) {
    return;
  }
  if (ec.failed()) {
    number_of_errors_++;
    InitiateSocket(index);
    return;
  }
  if (buf[3] != 0x00) {
    number_of_errors_++;
    InitiateSocket(index);
    return;
  }
  async_read(sock, mutable_buffer(buf, k2ndBindResponseSize),
             bind(&ClientSessionPool::Handle2ndResp, shared_from_this(), index,
                  placeholders::bytes_transferred, placeholders::error));
  // If this is the last socket in the pool, and the pool is not hydrated yet,
  // continue to hydrate.
  if (index == pool_.size() - 1 && pool_.size() < pool_size_) {
    pool_.emplace_back(client_sock_.get_executor());
    auto& next_sock = pool_[index + 1];
    next_sock.async_connect(
        proxy_endpoint_,
        bind(&ClientSessionPool::HandleConnect, shared_from_this(), index + 1,
             placeholders::error));
  }
}

void ClientSessionPool::Handle2ndResp(size_t index, size_t bytes_read,
                                      const error_code& ec) {
  auto& buf = buffers_[index].data;
  if (stop_.load()) {
    return;
  }
  if (ec.failed() || bytes_read < k2ndBindResponseSize || buf[1] != 0x00) {
    number_of_errors_++;
    InitiateSocket(index);
    return;
  }
  socket_vendor::NewConnectionResponse new_conn_resp;
  // Response is in the following format: address starts at byte 4.
  // +----+-----+-------+------+----------+----------+
  // |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  // +----+-----+-------+------+----------+----------+
  // | 1  |  1  | X'00' |  1   | Variable |    2     |
  // +----+-----+-------+------+----------+----------+
  memcpy(&new_conn_resp.addr, &buf[4], 16);  // assume IPv6 address
  memcpy(&new_conn_resp.port, &buf[20], 2);
  memcpy(buf, &new_conn_resp, sizeof(new_conn_resp));
  client_sock_.async_wait(
      Socket::wait_write,
      bind_executor(client_strand_,
                    bind(&ClientSessionPool::HandleSendFd, shared_from_this(),
                         index, placeholders::error)));
}

void ClientSessionPool::HandleSendFd(size_t index, const error_code& ec) {
  auto& sock = pool_[index];
  auto& buf = buffers_[index].data;
  if (stop_.load()) {
    return;
  }
  if (ec.failed()) {
    number_of_errors_++;
    InitiateSocket(index);
    return;
  }
  struct msghdr msg = {};
  struct iovec iov = {buf, sizeof(socket_vendor::NewConnectionResponse)};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  union {
    struct msghdr align;
    char buf[CMSG_SPACE(sizeof(int))];
  } cmsgu;

  msg.msg_control = cmsgu.buf;
  msg.msg_controllen = sizeof(cmsgu.buf);
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  int fd = sock.native_handle();
  memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

  ssize_t bytes_sent = sendmsg(client_sock_.native_handle(), &msg, 0);
  // If client_sock_ write fails, we should stop the whole pool.
  if (bytes_sent <= 0) {
    Stop();
    return;
  }
  // Now the socket fd is sent, we should be safe to close it on our side and
  // start a new one.
  InitiateSocket(index);
}

void ClientSessionPool::HandleClientError(const error_code& ec) {
  if (!stop_.load()) {
    Stop();
  }
}

void ClientSessionPool::Stop() {
  stop_.store(true);
  auto closure = [this]() {
    error_code ec;
    for (auto& sock : pool_) {
      sock.close(ec);
    }
    client_sock_.close();
  };
  asio::dispatch(bind_executor(client_strand_, closure));
}

}  // namespace google::scp::proxy
