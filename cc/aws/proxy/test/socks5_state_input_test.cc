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

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <unistd.h>

#include "proxy/src/socks5_state.h"

namespace google::scp::proxy::test {
using BufferUnitType = uint8_t;

class Socks5StateInputTest : public ::testing::Test {
 public:
  void SetState(Socks5State& state, Socks5State::HandshakeState state_val) {
    state.SetState(state_val);
  }

  void SetRequiredSize(Socks5State& state, size_t size) {
    state.SetRequiredSize(size);
  }
};

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

// The greeting is defined by RFC1928 as:
//   +----+----------+----------+
//   |VER | NMETHODS | METHODS  |
//   +----+----------+----------+
//   | 1  |    1     | 1 to 255 |
//   +----+----------+----------+
TEST_F(Socks5StateInputTest, GreetingHeaderBadVer) {
  BufferUnitType buffer[] = {0xab, 0x01};  // 1st byte should always be 0x05
  Socks5State state;
  SetState(state, Socks5State::kGreetingHeader);
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateInputTest, GreetingHeaderZeroMethods) {
  BufferUnitType buffer[] = {0x05, 0x00};
  Socks5State state;
  SetState(state, Socks5State::kGreetingHeader);
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateInputTest, GreetingMethodsNoValid) {
  BufferUnitType buffer[] = {0x01, 0x02, 0x03};
  Socks5State state;
  SetState(state, Socks5State::kGreetingMethods);
  SetRequiredSize(state, sizeof(buffer));
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

// The request is defined by RFC1928 as:
//   +----+-----+-------+------+----------+----------+
//   |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
//   +----+-----+-------+------+----------+----------+
//   | 1  |  1  | X'00' |  1   | Variable |    2     |
//   +----+-----+-------+------+----------+----------+

TEST_F(Socks5StateInputTest, RequestHeaderBadVer) {
  BufferUnitType buffer[] = {0xef, 0x01, 0x00, 0x04};
  Socks5State state;
  SetState(state, Socks5State::kRequestHeader);
  SetRequiredSize(state, sizeof(buffer));
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateInputTest, RequestHeaderBadCmd) {
  BufferUnitType buffer[] = {0x05, 0x06, 0x00, 0x04};
  Socks5State state;
  SetState(state, Socks5State::kRequestHeader);
  SetRequiredSize(state, sizeof(buffer));
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateInputTest, RequestHeaderBadRsv) {
  BufferUnitType buffer[] = {0x05, 0x01, 0xcc, 0x04};
  Socks5State state;
  SetState(state, Socks5State::kRequestHeader);
  SetRequiredSize(state, sizeof(buffer));
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateInputTest, RequestHeaderAtyp) {
  BufferUnitType buffer[] = {0x05, 0x01, 0x00, 0xdd};
  Socks5State state;
  SetState(state, Socks5State::kRequestHeader);
  SetRequiredSize(state, sizeof(buffer));
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateInputTest, RequestAddrV4Refusal) {
  // IPv4 address: 127.0.0.9:2048
  BufferUnitType buffer[] = {127, 0, 0, 9, 8, 00};
  Socks5State state;
  state.SetConnectCallback(
      [](const sockaddr*, size_t) { return Socks5State::kStatusFail; });
  SetState(state, Socks5State::kRequestAddrV4);
  SetRequiredSize(state, sizeof(buffer));
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}

TEST_F(Socks5StateInputTest, RequestAddrV6Refusal) {
  // IPv6 address: [0000:0000:1234:5678:9ABC:DEF0:1234:5678]:2048
  BufferUnitType buffer[] = {0x00, 0x00, 0x00, 0x00, 0x12, 0x34,
                             0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
                             0x12, 0x34, 0x56, 0x78, 0x08, 0x00};
  Socks5State state;
  state.SetConnectCallback(
      [](const sockaddr*, size_t) { return Socks5State::kStatusFail; });
  SetState(state, Socks5State::kRequestAddrV6);
  SetRequiredSize(state, sizeof(buffer));
  Buffer real_buff;
  real_buff.CopyIn(buffer, sizeof(buffer));
  EXPECT_FALSE(state.Proceed(real_buff));
  EXPECT_EQ(state.state(), Socks5State::kFail);
}
}  // namespace google::scp::proxy::test
