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

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <nghttp2/asio_http2_server.h>

#include "cc/core/interface/http_server_interface.h"

namespace google::scp::core {
/**
 * @brief Wrapper object of a nghttp2::response object to interface with it.
 */
class NgHttp2Response : public HttpResponse {
 public:
  explicit NgHttp2Response(
      const nghttp2::asio_http2::server::response& ng2_response)
      : ng2_response_(ng2_response), is_closed_(false) {}

  /**
   * @brief Callback for connection closing.
   * uint32_t error code of connection closure.
   */
  using OnCloseErrorCode = uint32_t;
  using OnCloseCallback = std::function<void(OnCloseErrorCode)>;

  /**
   * @brief Submit work onto the IoService of the response.
   *
   * @param work
   */
  void SubmitWorkOnIoService(std::function<void()> work) noexcept;

  /**
   * @brief Sends the populated response back to the client.
   * NOTE: Should always be invoked on a thread that belongs to nghttp2
   * response. A way to do this is to post this invocation as a work onto the
   * IoService of nghttp2 response object.
   */
  void Send() noexcept;

  /**
   * @brief Set callback to be invoked when connection is closing on the
   * response.
   *
   * @param callback
   */
  void SetOnCloseCallback(const OnCloseCallback& callback);

 private:
  /**
   * @brief Internal handler called when the connection is closing.
   *
   * @param error_code The error code for closing
   * @param callback callback to be invoked once the connection closes on the
   * response.
   */
  void OnClose(uint32_t error_code, OnCloseCallback callback) noexcept;

  /// A reference to the ng2 response object.
  const nghttp2::asio_http2::server::response& ng2_response_;

  /// Indicates whether the response stream is closed.
  bool is_closed_;

  /// Mutex to synchronize on_close while sending response back on connection.
  std::mutex on_close_mutex_;
};
}  // namespace google::scp::core
