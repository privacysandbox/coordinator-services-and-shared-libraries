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

#include "proxy/src/client_session_pool.h"

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include "proxy/src/logging.h"
#include "proxy/src/socket_vendor_protocol.h"

using UdsSocket = boost::asio::local::stream_protocol::socket;
namespace asio = boost::asio;
using asio::const_buffer;
using asio::mutable_buffer;
using boost::system::error_code;
using std::make_shared;
using std::move;
using std::thread;

namespace google::scp::proxy::test {

TEST(ClientSessionPoolTest, TestBind) {
  asio::io_context io_context;
  UdsSocket client_sock0(io_context);
  UdsSocket client_sock1(io_context);
  asio::local::connect_pair(client_sock0, client_sock1);

  char tmpfile[] = "/tmp/client_session_test_XXXXXX";
  mktemp(tmpfile);
  ASSERT_NE(tmpfile[0], '\0');
  asio::local::stream_protocol::endpoint ep(tmpfile);
  asio::local::stream_protocol::acceptor acceptor(io_context);
  acceptor.open(ep.protocol());
  acceptor.set_option(Acceptor::reuse_address(true));
  error_code ec;
  acceptor.bind(ep, ec);
  ASSERT_FALSE(ec.failed()) << "Bind error: " << ec.message();
  acceptor.listen();

  Endpoint local_ep(acceptor.local_endpoint());

  auto pool = make_shared<ClientSessionPool>(move(client_sock1), local_ep);
  thread initiator([pool]() { EXPECT_TRUE(pool->Start()); });

  auto work = asio::make_work_guard(io_context);
  thread worker([&io_context]() {
    auto n = io_context.run();
    LogInfo("Number of handlers: ", n);
  });

  socket_vendor::BindRequest bind_req;
  bind_req.port = 0x1234;  // just a fake port to test the responses
  asio::write(client_sock0, const_buffer(&bind_req, sizeof(bind_req)), ec);
  ASSERT_FALSE(ec.failed()) << "Bind request error: " << ec.message();
  socket_vendor::ListenRequest listen_req;
  listen_req.backlog = 64;
  asio::write(client_sock0, const_buffer(&listen_req, sizeof(listen_req)), ec);
  ASSERT_FALSE(ec.failed()) << "Bind request error: " << ec.message();

  pid_t client_pid = fork();
  if (client_pid == 0) {
    socket_vendor::BindResponse bind_resp;
    asio::read(client_sock0, mutable_buffer(&bind_resp, sizeof(bind_resp)), ec);
    if (ec.failed()) {
      exit(1);
    }
    socket_vendor::ListenResponse listen_resp;
    asio::read(client_sock0, mutable_buffer(&listen_resp, sizeof(listen_resp)),
               ec);
    if (ec.failed()) {
      exit(1);
    }
    // Try to receive a FD
    uint8_t client_buff[128];
    struct msghdr msg = {};
    struct iovec iov = {client_buff, sizeof(client_buff)};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union {
      struct cmsghdr align;
      char buf[CMSG_SPACE(sizeof(int))];
    } cmsgu;

    msg.msg_control = cmsgu.buf;
    msg.msg_controllen = sizeof(cmsgu.buf);
    // client_sock0.wait(Socket::wait_read);
    ssize_t bytes_recv = recvmsg(client_sock0.native_handle(), &msg, 0);
    if (bytes_recv <= 0) {
      exit(2);
    }
    LogInfo("Client received ", bytes_recv, " bytes");
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr) {
      exit(3);
    }
    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
      exit(4);
    }
    int fd = -1;
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
    if (fd <= 0) {
      exit(5);
    }
    exit(0);
  }

  asio::local::stream_protocol::socket proxy_client_sock(io_context);
  acceptor.accept(proxy_client_sock, ec);
  ASSERT_FALSE(ec.failed()) << "Proxy accept error: " << ec.message();

  uint8_t buff[13];
  asio::read(proxy_client_sock, mutable_buffer(buff, sizeof(buff)), ec);
  ASSERT_FALSE(ec.failed()) << "Proxy read error: " << ec.message();
  static constexpr uint8_t proxy_bind_resp[] = {
      0x05, 0x00,              // <- Greeting response
      0x05, 0x00, 0x00, 0x01,  // <- request header
      0x00, 0x00, 0x00, 0x00,  // <- addr = 0.0.0.0
      0x12, 0x34};             // <- port, to be assigned
  asio::write(proxy_client_sock,
              const_buffer(proxy_bind_resp, sizeof(proxy_bind_resp)), ec);
  ASSERT_FALSE(ec.failed()) << "Proxy write error: " << ec.message();

  static constexpr uint8_t bind_2nd_resp[] = {
      0x05, 0x00, 0x00, 0x04,  // <- response header
      // IPv6 addr [::1]
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,  //
      0x04, 0x00};                                     // <- port = 1024
  asio::write(proxy_client_sock,
              const_buffer(bind_2nd_resp, sizeof(bind_2nd_resp)), ec);
  ASSERT_FALSE(ec.failed()) << "Proxy write error: " << ec.message();

  int wstatus = 0;
  waitpid(client_pid, &wstatus, 0);
  EXPECT_TRUE(WIFEXITED(wstatus));
  EXPECT_EQ(WEXITSTATUS(wstatus), 0);
  initiator.join();
  work.reset();
  pool->Stop();
  worker.join();
  unlink(tmpfile);
}

}  // namespace google::scp::proxy::test
