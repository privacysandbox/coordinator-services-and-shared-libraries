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

#ifndef SOCKS5_STATE_H_
#define SOCKS5_STATE_H_

#include <assert.h>
#include <stdint.h>
#include <sys/socket.h>

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "proxy/src/buffer.h"

namespace google::scp::proxy {
namespace test {
class Socks5StateInputTest;
}  // namespace test

// A state machine that processes the SOCKS5 handshake. Thread-safety: unsafe.
// User is required to add synchronization/locking mechanism to ensure safety.
class Socks5State {
 public:
  // The internal handshake state of socks5 protocol.
  enum HandshakeState {
    kGreetingHeader,
    kGreetingMethods,
    kRequestHeader,
    kRequestAddrV4,
    kRequestAddrV6,
    kRequestBind,
    kWaitConnect,
    kWaitAccept,
    kResponse,
    kSuccess,
    kFail
  };

  // The return status of callbacks.
  enum CallbackStatus {
    kStatusOK,          // The call succeeded.
    kStatusInProgress,  // No error yet but need more data or additional action
    kStatusFail         // There was an error
  };

  // Callback to be called when we need to send response data to client.
  using ResponseCallback = std::function<CallbackStatus(const void*, size_t)>;
  // Callback to be called when we need to connect to destination.
  using ConnectCallback =
      std::function<CallbackStatus(const sockaddr*, size_t)>;
  // Callback to be called when we need to obtain address to send in the final
  // response. remote indicate if we request to get the remote address or local
  // address on dest socket.
  using DestAddressCallback =
      std::function<CallbackStatus(sockaddr*, size_t*, bool remote)>;
  // Callback to be called when we need to bind to a local port as requested by
  // BIND command. port is set to the port value bound.
  using BindCallback = std::function<CallbackStatus(uint16_t& port)>;

  Socks5State()
      : state_(HandshakeState::kGreetingHeader),  // Start with client greeting
        required_size_(2)  // Read byte 2 to reveal the length of the greeting.
  {}

  ~Socks5State();

  // Set the callback to be called when we need to send response data to client.
  void SetResponseCallback(ResponseCallback callback) {
    response_callback_ = std::move(callback);
  }

  // Set the callback to be called when we need to connect to destination.
  void SetConnectCallback(ConnectCallback callback) {
    connect_callback_ = std::move(callback);
  }

  // Set the callback to be called when we need to obtain local address to send
  // in the final response.
  void SetDestAddressCallback(DestAddressCallback callback) {
    dest_address_callback_ = std::move(callback);
  }

  void SetBindCallback(BindCallback callback) {
    bind_callback_ = std::move(callback);
  }

  // For application to call when previous in-progress connection to remote
  // succeeded. Return true if subsequent handshake states succeeded.
  bool ConnectionSucceed();

  // Perform one state transition.  Return true if state transition is made
  // without failure. Otherwise return false.
  bool Proceed(Buffer& buffer);

  // Return the internal state machine state.
  HandshakeState state() const { return state_; }

  bool InsufficientBuffer(const Buffer& buffer) const {
    return buffer.data_size() < required_size_;
  }

  bool Failed() const {
    return state_ < Socks5State::kGreetingHeader ||
           state_ > Socks5State::kSuccess;
  }

 private:
  // A bit ugly to define all of these friends, but that's the way how gtest
  // works, when we want to test private methods.
  friend class test::Socks5StateInputTest;

  // Helper functions for testing purposes.
  void SetState(HandshakeState state) { state_ = state; }

  void SetRequiredSize(size_t size) { required_size_ = size; }

  // Create a final socks5 response according to current socks5 state.
  std::vector<uint8_t> CreateResp(bool bind);

  ResponseCallback response_callback_;
  ConnectCallback connect_callback_;
  DestAddressCallback dest_address_callback_;
  BindCallback bind_callback_;

  // The state of the socks5 handshake.
  HandshakeState state_;
  // Required minimum size of data to consume to complete current state.
  size_t required_size_;
};
}  // namespace google::scp::proxy

#endif  // SOCKS5_STATE_H_
