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

#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>

#include <memory>
#include <thread>
#include <vector>

#include "proxy/src/logging.h"
#include "proxy/src/protocol.h"

using std::vector;

namespace google::scp::proxy {
static const size_t kBufferSize = 65536;

vector<uint8_t> Socks5State::CreateResp(bool is_bind) {
  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  // Per rfc1928 the response is in this format:
  //  +----+-----+-------+------+----------+----------+
  //  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  //  +----+-----+-------+------+----------+----------+
  //  | 1  |  1  | X'00' |  1   | Variable |    2     |
  //  +----+-----+-------+------+----------+----------+
  // The size of a response with IPv4/IPv6 addresses are 10(v4), 22(v6),
  // respectively. So 32 should be large enough to contain them.
  static constexpr size_t kRespBufSize = 32;
  uint8_t resp_storage[kRespBufSize] = {0x05, 0x00, 0x00};
  size_t resp_size = 3;  // First 3 bytes are fixed as defined above.
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  if (dest_address_callback_ &&
      dest_address_callback_(addr, &addr_len, is_bind) == kStatusOK) {
    // Successful. Return response.
    size_t addr_sz = FillAddrPort(&resp_storage[resp_size], addr);
    resp_size += addr_sz;
    vector<uint8_t> resp(resp_storage, resp_storage + resp_size);
    return resp;
  }
  // Otherwise, we have an error.
  LogError("ERROR: failed to get local address. errno=", errno);
  // A template of error response, with REP = 0x01, and all address and port
  // bytes set to 0x00.
  static const uint8_t err_resp_template[] = {0x05, 0x01, 0x00, 0x01, 0x00,
                                              0x00, 0x00, 0x00, 0x00, 0x00};
  return vector<uint8_t>(err_resp_template,
                         err_resp_template + sizeof(err_resp_template));
}

Socks5State::~Socks5State() {}

bool Socks5State::Proceed(Buffer& buffer) {
  // TODO: Socks5State class should only handles buffers, not socket or threads.
  // The logic for socket operations and thread creation should be refactored
  // out.
  if (InsufficientBuffer(buffer)) {
    return false;
  }
  // TODO: refactor this long switch case to improve readability.
  switch (state_) {
    case Socks5State::kGreetingHeader: {
      // Per RFC1928 https://datatracker.ietf.org/doc/html/rfc1928
      //   +----+----------+----------+
      //   |VER | NMETHODS | METHODS  |
      //   +----+----------+----------+
      //   | 1  |    1     | 1 to 255 |
      //   +----+----------+----------+
      // Since we don't support authentication, we expect to read NMETHODS ==
      // 1, and METHODS = 0x00 (no auth required).
      uint8_t greeting[2];
      buffer.CopyOut(greeting, sizeof(greeting));
      if (greeting[0] != 0x05 || greeting[1] == 0) {
        char s[32] = {0};
        sprintf(s, "%#04x %#04x\n", greeting[0], greeting[1]);
        LogError("Malformed client greeting: ", s);
        state_ = Socks5State::kFail;
        break;
      }
      required_size_ = greeting[1];
      state_ = Socks5State::kGreetingMethods;
      break;
    }
    case Socks5State::kGreetingMethods: {
      size_t n_methods = required_size_;
      // TODO: handle bad_alloc
      std::unique_ptr<uint8_t[]> methods(new uint8_t[n_methods]);
      buffer.CopyOut(methods.get(), n_methods);
      // We only support "no auth" here, which is represented by 0x00.
      if (memchr(methods.get(), 0x00, n_methods) == nullptr) {
        LogError("Unsupported auth methods.");
        state_ = Socks5State::kFail;
        break;
      }
      static const uint8_t kGreetingResp[2] = {0x05, 0x00};
      if (response_callback_) {
        // Our response is tiny, just 2 bytes. There's no chance for the send to
        // block and require us to poll. So for simplicity, we just check if
        // return is kStatusOK.
        auto ret = response_callback_(kGreetingResp, sizeof(kGreetingResp));
        if (ret != kStatusOK) {
          LogError("Handshake failure with client.");
          state_ = Socks5State::kFail;
          break;
        }
      }
      state_ = Socks5State::kRequestHeader;
      required_size_ = 4;
      break;
    }
    case Socks5State::kRequestHeader: {
      // The request is defined by RFC1928 as:
      //   +----+-----+-------+------+----------+----------+
      //   |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
      //   +----+-----+-------+------+----------+----------+
      //   | 1  |  1  | X'00' |  1   | Variable |    2     |
      //   +----+-----+-------+------+----------+----------+
      uint8_t header[4];
      buffer.CopyOut(header, sizeof(header));
      if (header[0] != 0x05 || header[2] != 0x00) {
        LogError("Malformed client request.");
        // TODO: return meaningful response to client.
        state_ = Socks5State::kFail;
        break;
      }
      if (header[1] == 0x01) {  // CMD == CONNECT
        uint8_t atyp = header[3];
        if (atyp == 0x01) {
          state_ = Socks5State::kRequestAddrV4;
          required_size_ = 6;  // 4-byte IPv4 address, 2-byte port
        } else if (atyp == 0x03) {
          LogError("Unsupported ATYP: 0x03(fqdn)");
          state_ = Socks5State::kFail;
          break;
        } else if (atyp == 0x04) {
          state_ = Socks5State::kRequestAddrV6;
          required_size_ = 16 + 2;  // 16-byte IPv6 address, 2-byte port
        } else {
          LogError("Malformed client request. ATYP = ", atyp);
          state_ = Socks5State::kFail;
          break;
        }
      } else if (header[1] == 0x02) {  // CMD == BIND
        uint8_t atyp = header[3];
        if (atyp == 0x01) {
          required_size_ = 6;  // 4-byte IPv4 address, 2-byte port
        } else if (atyp == 0x03) {
          LogError("Unsupported ATYP: 0x03(fqdn)");
          state_ = Socks5State::kFail;
          break;
        } else if (atyp == 0x04) {
          required_size_ = 16 + 2;  // 16-byte IPv6 address, 2-byte port
        } else {
          LogError("Malformed client request. ATYP = ", atyp);
          state_ = Socks5State::kFail;
          break;
        }
        state_ = Socks5State::kRequestBind;
        break;
      } else {
        LogError("Malformed client request.");
        // TODO: return meaningful response to client.
        state_ = Socks5State::kFail;
        break;
      }
      break;
    }
    case Socks5State::kRequestAddrV4: {
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      // The addr and port are already in network byte order, so just copy
      buffer.CopyOut(&addr.sin_addr, sizeof(addr.sin_addr));
      buffer.CopyOut(&addr.sin_port, sizeof(addr.sin_port));
      // No matter what, we require no more data from client.
      required_size_ = 0;
      if (!connect_callback_) {
        required_size_ = 0;
        state_ = Socks5State::kWaitConnect;
        break;
      }
      auto ret =
          connect_callback_(reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      if (ret == kStatusOK) {
        state_ = Socks5State::kResponse;
        break;
      }
      if (ret == kStatusInProgress) {
        state_ = Socks5State::kWaitConnect;
        return false;
        break;
      }
      state_ = Socks5State::kFail;
      break;
    }
    case Socks5State::kRequestAddrV6: {
      // Similar to kRequestAddrV4, except that it is a v6 address
      struct sockaddr_in6 addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin6_family = AF_INET6;
      // The addr and port are already in network byte order, so just copy
      buffer.CopyOut(&addr.sin6_addr, sizeof(addr.sin6_addr));
      buffer.CopyOut(&addr.sin6_port, sizeof(addr.sin6_port));
      // No matter what, we require no more data from client.
      required_size_ = 0;
      if (!connect_callback_) {
        state_ = Socks5State::kWaitConnect;
        break;
      }
      auto ret =
          connect_callback_(reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      if (ret == kStatusOK) {
        state_ = Socks5State::kResponse;
        break;
      }
      if (ret == kStatusInProgress) {
        state_ = Socks5State::kWaitConnect;
        return false;
        break;
      }
      state_ = Socks5State::kFail;
      break;
    }
    case Socks5State::kRequestBind: {
      uint16_t port = 0;
      // We don't care what address to bind, we'll bind to default [::] anyway.
      // So just discard the address field.
      buffer.CopyOut(nullptr, required_size_ - sizeof(port));
      buffer.CopyOut(&port, sizeof(port));
      port = ntohs(port);
      required_size_ = 0;
      auto ret = bind_callback_(port);
      if (ret == kStatusFail) {
        state_ = Socks5State::kFail;
        break;
      }
      // We are using IPv4 ATYP for both IPv4 and IPv6 bind requests. This seems
      // to be OK, as we are going to send all zeros in address anyway.
      // NOTE: if you change this, you'll also need to change the client side
      // (ClientSessionPool).
      uint8_t bind_resp[] = {0x05, 0x00, 0x00, 0x01, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00};
      // Fill in the port field
      port = htons(port);
      memcpy(bind_resp + sizeof(bind_resp) - sizeof(port), &port, sizeof(port));
      ret = response_callback_(bind_resp, sizeof(bind_resp));
      if (ret == kStatusFail) {
        state_ = Socks5State::kFail;
        break;
      }
      state_ = Socks5State::kWaitAccept;
      return false;
      break;
    }
    case Socks5State::kWaitConnect: {
      // Always return false. The application is expected to call
      // ConnectionSucceed() upon successful connection establishment.
      return false;
      break;
    }
    case Socks5State::kWaitAccept: {
      // Always return false. The application is expected to call
      // ConnectionSucceed() upon successful connection establishment.
      return false;
      break;
    }
    case Socks5State::kResponse: {
      auto resp = CreateResp(false);
      // Again, like kGreetingMethods, our response is tiny. There's no chance
      // for the send to block and require us to poll. So for simplicity, we
      // just check if return is kStatusOK.
      auto ret = response_callback_(resp.data(), resp.size());
      if (ret != kStatusOK) {
        state_ = Socks5State::kFail;
        break;
      }
      state_ = Socks5State::kSuccess;
      required_size_ = 1;
      break;
    }
    case Socks5State::kSuccess: {
      return false;
      break;
    }
    case Socks5State::kFail: {
      required_size_ = 0;
      return false;
    }
    default: {
      return false;
    }
  }  // switch
  return state_ != Socks5State::kFail;
}

bool Socks5State::ConnectionSucceed() {
  vector<uint8_t> resp;
  if (state_ == kWaitConnect) {
    resp = CreateResp(false /* is_bind */);
  } else if (state_ == kWaitAccept) {
    resp = CreateResp(true /* is_bind */);
  } else {
    state_ = kFail;
    return false;
  }
  // Again, like kGreetingMethods, our response is tiny. There's no chance
  // for the send to block and require us to poll. So for simplicity, we
  // just check if return is kStatusOK.
  auto ret = response_callback_(resp.data(), resp.size());
  if (ret != kStatusOK) {
    state_ = Socks5State::kFail;
  }
  state_ = Socks5State::kSuccess;
  return state_ == Socks5State::kSuccess;
}

}  // namespace google::scp::proxy
