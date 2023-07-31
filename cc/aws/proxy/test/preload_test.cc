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

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/vm_sockets.h>

class PreloadSyscallTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fd_ = socket(AF_VSOCK, SOCK_STREAM, 0);
    ASSERT_GT(fd_, 0);
  }

  void TearDown() override { close(fd_); }

  int fd_;
};

// Test that we can call ioctl(FIONREAD) on a VSOCK socket.
TEST_F(PreloadSyscallTest, ioctl) {
  int sz = 0;

  int rc = ioctl(fd_, FIONREAD, &sz);

  EXPECT_EQ(rc, 0);
  EXPECT_EQ(sz, 0);
}

// Test that we can call getsockopt(SOL_TCP/IPPROTO_TCP) on a VSOCK socket.
TEST_F(PreloadSyscallTest, getsockopt) {
  int val = 0;
  socklen_t sz = 0;

  int rc = getsockopt(fd_, SOL_TCP, TCP_NODELAY, &val, &sz);
  EXPECT_EQ(rc, 0);

  rc = getsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &val, &sz);
  EXPECT_EQ(rc, 0);
}

TEST_F(PreloadSyscallTest, setsockopt) {
  int val = 1;

  int rc = setsockopt(fd_, SOL_TCP, TCP_NODELAY, &val, sizeof(val));
  EXPECT_EQ(rc, 0);

  rc = setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
  EXPECT_EQ(rc, 0);
}

// TODO: add tests for connect(), socks5_connect() when the server side logic is
// cleaned up, so that we can contain them in unit tests. For now they are
// tested via e2e tests.
