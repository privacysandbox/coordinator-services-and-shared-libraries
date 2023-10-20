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

#include "proxy/src/protocol.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

TEST(ProtocolTest, FillAddrPortV4) {
  static const char ipv4_addr[] = "12.34.56.78";
  static const uint16_t port = 0x1234;
  static const uint8_t expected[] = {0x01, 12, 34, 56, 78, 0x12, 0x34};
  sockaddr_in v4addr;
  v4addr.sin_family = AF_INET;
  v4addr.sin_port = htons(port);
  inet_aton(ipv4_addr, &v4addr.sin_addr);
  uint8_t buffer[64];
  EXPECT_EQ(FillAddrPort(buffer, reinterpret_cast<sockaddr*>(&v4addr)),
            sizeof(expected));
  EXPECT_EQ(memcmp(buffer, expected, sizeof(expected)), 0);
}

TEST(ProtocolTest, FillAddrPortV6) {
  static const char ipv6_addr[] = "1234:5678:90ab:cdef:1234:5678:90ab:cdef";
  static const uint16_t port = 0x1234;
  static const uint8_t expected[] = {0x04, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab,
                                     0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90,
                                     0xab, 0xcd, 0xef, 0x12, 0x34};
  sockaddr_in6 v6addr;
  v6addr.sin6_family = AF_INET6;
  v6addr.sin6_port = htons(port);
  inet_pton(AF_INET6, ipv6_addr, &v6addr.sin6_addr);
  uint8_t buffer[64];
  EXPECT_EQ(FillAddrPort(buffer, reinterpret_cast<sockaddr*>(&v6addr)),
            sizeof(expected));
  EXPECT_EQ(memcmp(buffer, expected, sizeof(expected)), 0);
}
