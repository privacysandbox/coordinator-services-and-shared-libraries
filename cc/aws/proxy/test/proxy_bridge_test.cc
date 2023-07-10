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

#include "proxy/src/proxy_bridge.h"

#include <gtest/gtest.h>

#include <stdint.h>
#include <sys/socket.h>

#include <memory>
#include <thread>

#include <boost/asio.hpp>

using boost::system::error_code;
using std::make_shared;
using std::make_unique;
using std::move;
using std::thread;
using UdsSocket = boost::asio::local::stream_protocol::socket;

namespace asio = boost::asio;

namespace google::scp::proxy::test {

TEST(ProxyBridge, EmptyConnection) {
  asio::io_context io_context;
  UdsSocket client_sock0(io_context);
  UdsSocket client_sock1(io_context);
  UdsSocket dest_sock0(io_context);
  UdsSocket dest_sock1(io_context);
  asio::local::connect_pair(client_sock0, client_sock1);
  asio::local::connect_pair(dest_sock0, dest_sock1);
  int client_sock_fd = client_sock1.native_handle();
  int dest_sock_fd = dest_sock1.native_handle();
  {
    auto bridge =
        make_shared<ProxyBridge>(move(client_sock1), move(dest_sock1));
    bridge->ForwardTraffic();
  }

  thread worker_thread([&]() { io_context.run(); });
  dest_sock0.close();
  uint8_t dummy_buff[64];
  error_code ec;
  size_t sz = client_sock0.read_some(asio::buffer(dummy_buff), ec);

  EXPECT_TRUE(ec.failed());
  EXPECT_EQ(sz, 0);
  client_sock0.close();

  worker_thread.join();

  int ret = fcntl(client_sock_fd, F_GETFD);
  EXPECT_EQ(ret, -1) << "fd=" << client_sock_fd << "is still open";
  ret = fcntl(dest_sock_fd, F_GETFD);
  EXPECT_EQ(ret, -1) << "fd=" << dest_sock_fd << "is still open";
}

TEST(ProxyBridge, HalfClosure) {
  asio::io_context io_context;
  UdsSocket client_sock0(io_context);
  UdsSocket client_sock1(io_context);
  UdsSocket dest_sock0(io_context);
  UdsSocket dest_sock1(io_context);
  asio::local::connect_pair(client_sock0, client_sock1);
  asio::local::connect_pair(dest_sock0, dest_sock1);
  int client_sock_fd = client_sock1.native_handle();
  int dest_sock_fd = dest_sock1.native_handle();
  {
    auto bridge =
        make_shared<ProxyBridge>(move(client_sock1), move(dest_sock1));
    bridge->ForwardTraffic();
  }

  thread worker_thread([&]() { io_context.run(); });
  // Send FIN
  dest_sock0.shutdown(Socket::shutdown_send);
  uint8_t recv_buff[64];
  error_code ec;
  // Verify read failure on the other end;
  size_t sz = client_sock0.read_some(asio::buffer(recv_buff), ec);
  EXPECT_TRUE(ec.failed());
  EXPECT_EQ(sz, 0);

  uint8_t send_buff[] = "foo bar hello world easy peasy lemon squeezy";
  // Verify we can still send in the other direction on half-closed socket.
  client_sock0.send(asio::buffer(send_buff), 0, ec);
  EXPECT_FALSE(ec.failed());
  client_sock0.close();
  sz = dest_sock0.receive(asio::buffer(recv_buff), 0, ec);
  EXPECT_EQ(sz, sizeof(send_buff));

  worker_thread.join();

  int ret = fcntl(client_sock_fd, F_GETFD);
  EXPECT_EQ(ret, -1) << "fd=" << client_sock_fd << "is still open";
  ret = fcntl(dest_sock_fd, F_GETFD);
  EXPECT_EQ(ret, -1) << "fd=" << dest_sock_fd << "is still open";
}

TEST(ProxyBridge, ForwardTraffic) {
  asio::io_context io_context;
  UdsSocket client_sock0(io_context);
  UdsSocket client_sock1(io_context);
  UdsSocket dest_sock0(io_context);
  UdsSocket dest_sock1(io_context);
  asio::local::connect_pair(client_sock0, client_sock1);
  asio::local::connect_pair(dest_sock0, dest_sock1);
  int client_sock_fd = client_sock1.native_handle();
  int dest_sock_fd = dest_sock1.native_handle();

  {
    auto bridge =
        make_shared<ProxyBridge>(move(client_sock1), move(dest_sock1));
    bridge->ForwardTraffic();
  }

  auto send_buf = make_unique<uint8_t[]>(10 * 1024 * 1024);
  for (size_t i = 0; i < 10 * 1024 * 1024; ++i) {
    send_buf[i] = i & 0xff;
  }

  thread worker_thread([&]() { io_context.run(); });
  thread writer_thread([&]() {
    constexpr size_t buf_size = 10 * 1024 * 1024;
    error_code ec;
    asio::write(client_sock0, asio::buffer(send_buf.get(), buf_size), ec);
    client_sock0.close();
  });

  auto recv_buf = make_unique<uint8_t[]>(1024);
  constexpr size_t buf_size = 1024UL;
  size_t counter = 0UL;
  while (true) {
    error_code ec;
    auto sz = dest_sock0.read_some(asio::buffer(recv_buf.get(), buf_size), ec);
    for (auto i = 0u; i < sz; ++i) {
      EXPECT_EQ(recv_buf[i], counter++ & 0xff);
    }
    if (ec.failed()) {
      LogError("Reader error ", ec.value(), ": ", ec.what());
      LogError("Counter = ", counter);
      break;
    }
  }
  EXPECT_EQ(counter, 10 * 1024 * 1024);

  dest_sock0.close();
  writer_thread.join();
  worker_thread.join();

  int ret = fcntl(client_sock_fd, F_GETFD);
  EXPECT_EQ(ret, -1) << "fd=" << client_sock_fd << "is still open";
  ret = fcntl(dest_sock_fd, F_GETFD);
  EXPECT_EQ(ret, -1) << "fd=" << dest_sock_fd << "is still open";
}

TEST(ProxyBridge, InboundConnection) {
  asio::io_context io_context;
  UdsSocket client_sock0(io_context);
  UdsSocket client_sock1(io_context);
  asio::local::connect_pair(client_sock0, client_sock1);
  AcceptorPool acceptor_pool;

  {
    auto bridge = make_shared<ProxyBridge>(move(client_sock1), &acceptor_pool);
    bridge->PerformSocks5Handshake();
  }

  thread worker_thread([&]() {
    error_code ec;
    io_context.run(ec);
    EXPECT_FALSE(ec.failed()) << "io_context.run() failed: " << ec.message();
  });

  uint8_t data[] = {0x05, 0x01, 0x00,        // <- Greeting
                    0x05, 0x02, 0x00, 0x01,  // <- request header
                    0x7f, 0x00, 0x00, 0x01,  // <- addr = 127.0.0.1
                    0x00, 0x00};             // <- port = 0 to bind random port
  asio::write(client_sock0, asio::buffer(data));

  uint8_t buff[64];
  // Read greeting response
  asio::read(client_sock0, asio::buffer(buff, 2));
  // Read bind response
  asio::read(client_sock0, asio::buffer(buff, 10));
  uint16_t port = 0;
  memcpy(&port, buff + 8, 2);
  port = ntohs(port);
  EXPECT_GT(port, 1024);

  asio::ip::tcp::socket in_socket(io_context);
  asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), port);
  error_code ec;
  in_socket.connect(ep, ec);
  ASSERT_FALSE(ec.failed())
      << "connect failed: " << ec.message() << ". Port: " << port;

  // Read the last response when connection establishes
  asio::read(client_sock0, asio::buffer(buff, 8));

  auto send_buf = make_unique<uint8_t[]>(10 * 1024 * 1024);
  for (size_t i = 0; i < 10 * 1024 * 1024; ++i) {
    send_buf[i] = i & 0xff;
  }

  thread writer_thread([&]() {
    constexpr size_t buf_size = 10 * 1024 * 1024;
    error_code ec;
    asio::write(client_sock0, asio::buffer(send_buf.get(), buf_size), ec);
    client_sock0.close();
  });

  auto recv_buf = make_unique<uint8_t[]>(1024);
  constexpr size_t buf_size = 1024UL;
  size_t counter = 0UL;
  while (true) {
    error_code ec;
    auto sz = in_socket.read_some(asio::buffer(recv_buf.get(), buf_size), ec);
    for (auto i = 0u; i < sz; ++i) {
      EXPECT_EQ(recv_buf[i], counter++ & 0xff);
    }
    if (ec.failed()) {
      LogError("Counter = ", counter);
      break;
    }
  }
  EXPECT_EQ(counter, 10 * 1024 * 1024);

  io_context.stop();
  worker_thread.join();
  writer_thread.join();
}

}  // namespace google::scp::proxy::test
