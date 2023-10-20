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

#pragma once

#include <memory>
#include <string>

#include "core/http2_server/src/http2_request.h"

namespace google::scp::core::http2_server::mock {

class MockNgHttp2RequestWithOverrides : public NgHttp2Request {
 public:
  MockNgHttp2RequestWithOverrides(
      const nghttp2::asio_http2::server::request& ng2_request,
      size_t expected_request_body_length_to_receive = 1024)
      : NgHttp2Request(ng2_request) {
    body = BytesBuffer(expected_request_body_length_to_receive);
    expected_request_body_length_to_receive_ =
        expected_request_body_length_to_receive;
  }

  /**
   * @brief Simulates callbacks coming from nghttp2 containing request body
   * data.
   *
   * @param data
   * @param length
   */
  void SimulateFullRequestBodyDataReceived() {
    // Send full data
    BytesBuffer data_buffer(expected_request_body_length_to_receive_);
    uint8_t* data_buffer_ptr =
        reinterpret_cast<uint8_t*>(data_buffer.bytes->data());
    NgHttp2Request::OnRequestBodyDataChunkReceived(
        data_buffer_ptr, expected_request_body_length_to_receive_,
        on_request_body_received_);
    // Send a length=0 to indicate End of Stream
    NgHttp2Request::OnRequestBodyDataChunkReceived(data_buffer_ptr, 0,
                                                   on_request_body_received_);
  }

  /**
   * @brief Simulates callbacks coming from nghttp2 containing partial request
   * body data.
   *
   * @param data
   * @param length
   */
  void SimulatePartialRequestBodyDataReceived() {
    // Send only half of the data. Ensure
    // expected_request_body_length_to_receive_ is atleast 2 bytes.
    BytesBuffer data_buffer(expected_request_body_length_to_receive_);
    uint8_t* data_buffer_ptr =
        reinterpret_cast<uint8_t*>(data_buffer.bytes->data());
    NgHttp2Request::OnRequestBodyDataChunkReceived(
        data_buffer_ptr, expected_request_body_length_to_receive_ / 2,
        on_request_body_received_);
    // Send a length=0 to indicate End of Stream
    NgHttp2Request::OnRequestBodyDataChunkReceived(data_buffer_ptr, 0,
                                                   on_request_body_received_);
  }

  /**
   * @brief Simulates callbacks coming from nghttp2 containing request body
   * data.
   *
   * @param data
   * @param length
   */
  void SimulateOnRequestBodyDataReceived(const uint8_t* data,
                                         std::size_t length) {
    NgHttp2Request::OnRequestBodyDataChunkReceived(data, length,
                                                   on_request_body_received_);
  }

  /**
   * @brief Set the callback to be invoked when data receiving is complete.
   *
   * @param callback
   */
  void SetOnRequestBodyDataReceivedCallback(
      const RequestBodyDataReceivedCallback& callback) override {
    on_request_body_received_ = callback;
  }

  /**
   * @brief Check if SetOnRequestBodyDataReceivedCallback is invoked.
   *
   * @return true
   * @return false
   */
  bool IsOnRequestBodyDataReceivedCallbackSet() {
    return static_cast<bool>(on_request_body_received_);
  }

  /// @brief User supplied callback that will be invoked. Empty callback to
  /// begin with.
  RequestBodyDataReceivedCallback on_request_body_received_ =
      [](ExecutionResult) {};

  /// @brief Expected size of the data to be recevied.
  size_t expected_request_body_length_to_receive_;
};
}  // namespace google::scp::core::http2_server::mock
