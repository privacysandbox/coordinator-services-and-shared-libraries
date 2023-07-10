// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "proxy/src/socks5_state.h"

#include <gtest/gtest.h>

#include <stdint.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proxy/src/buffer.h"
#include "proxy/src/proxy_bridge.h"

namespace google::scp::proxy::test {
using BufferUnitType = uint8_t;

class Socks5StateTest : public testing::Test {
 public:
};

// A helper class to generate a socket pair and gracefully close with RAII.
class AutoCloseSocketPair {
 public:
  AutoCloseSocketPair() { socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd_); }

  ~AutoCloseSocketPair() {
    close(sockfd_[0]);
    close(sockfd_[1]);
  }

  int& operator[](int idx) { return sockfd_[idx]; }

 private:
  int sockfd_[2];
};

void HappyGreeting(void* data, size_t sz) {
  // Test greetings, happy path
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });

  Buffer buffer;
  buffer.CopyIn(data, sz);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kGreetingMethods);

  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestHeader);
  BufferUnitType resp[2];
  ssize_t ret_sz = recv(sockfd[1], resp, sizeof(resp), MSG_DONTWAIT);
  EXPECT_EQ(ret_sz, sizeof(resp));
  // Expected response: {0x05, 0x00}
  EXPECT_EQ(resp[0], 0x05) << "Bad response";
  EXPECT_EQ(resp[1], 0x00) << "Bad response";
}

void BadAuthMethod(void* data, size_t sz) {
  // Test greetings, with bad auth
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });

  Buffer buffer;
  buffer.CopyIn(data, sz);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kGreetingMethods);

  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kFail);
  BufferUnitType resp[2];
  ssize_t ret_sz = recv(sockfd[1], resp, sizeof(resp), MSG_DONTWAIT);
  // We expect the proxy either send nothing, or send {0x05, 0xff}
  EXPECT_TRUE(ret_sz == sizeof(resp) || ret_sz == -1);
  if (ret_sz == 2) {
    EXPECT_EQ(resp[0], 0x05) << "Bad response";
    EXPECT_EQ(resp[1], 0xff) << "Bad response";
  }
}

TEST_F(Socks5StateTest, NormalGreeting) {
  // Correct greeting should be {0x05, 0x01, 0x00}, indicating protocol
  // version 0x05, number of auth methods 0x0x1, and auth method 0x00 (no
  // auth)
  BufferUnitType buffer[] = {0x05, 0x01, 0x00};
  HappyGreeting(buffer, sizeof(buffer));
}

TEST_F(Socks5StateTest, BadGreeting) {
  Socks5State state;
  // Correct greeting should be {0x05, 0x01, 0x00}, indicating protocol version
  // 0x05, number of auth methods 0x0x1, and auth method 0x00 (no auth)
  BufferUnitType data[] = {0x05, 0x00, 0x00};
  Buffer buffer;
  buffer.CopyIn(data, sizeof(data));
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateTest, MultipleAuthMethods) {
  // Correct greeting should be as described in above tests. However, if the
  // client specifies multiple auth methods which includes 0x00, it should also
  // work.
  // This declares two auth methods, 0x03 and 0x00.
  BufferUnitType buffer[] = {0x05, 0x02, 0x03, 0x00};
  HappyGreeting(buffer, sizeof(buffer));
}

TEST_F(Socks5StateTest, UnsupportedMethods) {
  // Similar to test above. This declares two auth methods, 0x03 and 0x02. We
  // only support 0x00. So this should fail.
  BufferUnitType buffer[] = {0x05, 0x02, 0x03, 0x02};
  BadAuthMethod(buffer, sizeof(buffer));
}

TEST_F(Socks5StateTest, MaxNumMethods) {
  // Similar to tests above, but go extreme and fill up max number of methods,
  // including 0x00.
  BufferUnitType buffer[257] = {0x05, 0xFF};
  for (int m = 0; m < 0xff; ++m) {
    buffer[m + 2] = static_cast<uint8_t>(m);
  }
  HappyGreeting(buffer, sizeof(buffer));
}

TEST_F(Socks5StateTest, MaxNumMethodsReversed) {
  // Similar to MaxNumMethods, instead the methods are in reverse order.
  BufferUnitType buffer[258] = {0x05, 0xFF};
  for (int m = 0; m < 0xff; ++m) {
    buffer[m + 2] = static_cast<uint8_t>(0xfe - m);
  }
  HappyGreeting(buffer, sizeof(buffer));
}

TEST_F(Socks5StateTest, RequestV4) {
  BufferUnitType data[] = {0x05, 0x01, 0x00,        // <- Greeting
                           0x05, 0x01, 0x00, 0x01,  // <- request header
                           0x7f, 0x00, 0x00, 0x01,  // <- addr = 127.0.0.1
                           0x04, 0x00};             // <- port = 1024
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });

  state.SetConnectCallback(
      [](const sockaddr*, size_t len) { return Socks5State::kStatusOK; });
  state.SetDestAddressCallback(
      [](sockaddr*, size_t*, bool) { return Socks5State::kStatusOK; });

  Buffer buffer;
  buffer.CopyIn(data, sizeof(data));
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kGreetingMethods);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestHeader);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestAddrV4);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kResponse);
}

TEST_F(Socks5StateTest, RequestV6) {
  BufferUnitType data[] = {0x05, 0x01, 0x00,        // <- Greeting
                           0x05, 0x01, 0x00, 0x04,  // <- request header
                           // IPv6 addr [::1]
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04,
                           0x00};  // <- port = 1024
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });
  state.SetConnectCallback(
      [](const sockaddr*, size_t len) { return Socks5State::kStatusOK; });
  state.SetDestAddressCallback(
      [](sockaddr*, size_t*, bool) { return Socks5State::kStatusOK; });

  Buffer buffer;
  buffer.CopyIn(data, sizeof(data));
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kGreetingMethods);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestHeader);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestAddrV6);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kResponse);
}

TEST_F(Socks5StateTest, SlowClient) {
  // Here we borrow the correct requests from test RequestV6 above, and will
  // consume byte by byte to verify SOCKS5 state machine behaves correctly with
  // extremely slow clients.
  BufferUnitType data[] = {0x05, 0x01, 0x00,        // <- Greeting
                           0x05, 0x01, 0x00, 0x04,  // <- request header
                           // IPv6 addr [::1]
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,  //
                           0x04, 0x00};  // <- port = 1024
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });
  state.SetConnectCallback(
      [](const sockaddr*, size_t len) { return Socks5State::kStatusOK; });
  state.SetDestAddressCallback(
      [](sockaddr*, size_t*, bool) { return Socks5State::kStatusOK; });

  int idx = 0;
  Buffer buffer;
  buffer.CopyIn(&data[idx++], 1);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kGreetingHeader);
  EXPECT_TRUE(state.InsufficientBuffer(buffer));

  buffer.CopyIn(&data[idx++], 1);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kGreetingMethods);

  buffer.CopyIn(&data[idx++], 1);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestHeader);

  // For the next 3 bytes, we should not be able to make state transitions.
  for (int i = 0; i < 3; ++i) {
    buffer.CopyIn(&data[idx++], 1);
    EXPECT_FALSE(state.Proceed(buffer));
    EXPECT_TRUE(state.InsufficientBuffer(buffer));
    EXPECT_EQ(state.state(), Socks5State::kRequestHeader);
  }

  buffer.CopyIn(&data[idx++], 1);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestAddrV6);

  // Similarly, for the next 17 bytes, we should not be able to proceed, as IPv6
  // address + port is 18 bytes.
  for (int i = 0; i < 17; ++i) {
    buffer.CopyIn(&data[idx++], 1);
    EXPECT_FALSE(state.Proceed(buffer));
    EXPECT_TRUE(state.InsufficientBuffer(buffer));
    EXPECT_EQ(state.state(), Socks5State::kRequestAddrV6);
  }
  buffer.CopyIn(&data[idx++], 1);
  EXPECT_TRUE(state.Proceed(buffer));
  EXPECT_EQ(state.state(), Socks5State::kResponse);
}

TEST_F(Socks5StateTest, ConnectFailure) {
  BufferUnitType data[] = {0x05, 0x01, 0x00,        // <- Greeting
                           0x05, 0x01, 0x00, 0x01,  // <- request header
                           0x7f, 0x00, 0x00, 0x01,  // <- addr = 127.0.0.1
                           0x04, 0x00};             // <- port = 1024
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });

  state.SetConnectCallback(
      [](const sockaddr*, size_t len) { return Socks5State::kStatusFail; });

  Buffer buffer;
  buffer.CopyIn(data, sizeof(data));
  state.Proceed(buffer);
  state.Proceed(buffer);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestAddrV4);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateTest, ConnectInProgress) {
  BufferUnitType data[] = {0x05, 0x01, 0x00,        // <- Greeting
                           0x05, 0x01, 0x00, 0x01,  // <- request header
                           0x7f, 0x00, 0x00, 0x01,  // <- addr = 127.0.0.1
                           0x04, 0x00};             // <- port = 1024
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });

  state.SetConnectCallback([](const sockaddr*, size_t len) {
    return Socks5State::kStatusInProgress;
  });

  Buffer buffer;
  buffer.CopyIn(data, sizeof(data));
  state.Proceed(buffer);
  state.Proceed(buffer);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestAddrV4);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kWaitConnect);
  EXPECT_TRUE(state.ConnectionSucceed());
}

TEST_F(Socks5StateTest, Bind) {
  BufferUnitType data[] = {0x05, 0x01, 0x00,        // <- Greeting
                           0x05, 0x02, 0x00, 0x01,  // <- request header
                           0x7f, 0x00, 0x00, 0x01,  // <- addr = 127.0.0.1
                           0x10, 0x01};             // <- port = 4097
  AutoCloseSocketPair sockfd;
  Socks5State state;
  state.SetResponseCallback([sock = sockfd[0]](const void* data, size_t len) {
    if (send(sock, data, len, 0) != static_cast<ssize_t>(len)) {
      return Socks5State::kStatusFail;
    }
    return Socks5State::kStatusOK;
  });

  state.SetBindCallback(
      [](uint16_t port) { return Socks5State::kStatusInProgress; });

  Buffer buffer;
  buffer.CopyIn(data, sizeof(data));
  state.Proceed(buffer);
  state.Proceed(buffer);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kRequestBind);
  state.Proceed(buffer);
  EXPECT_EQ(state.state(), Socks5State::kWaitAccept);
  EXPECT_TRUE(state.ConnectionSucceed());
}

}  // namespace google::scp::proxy::test
