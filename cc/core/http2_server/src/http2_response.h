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
/// Nghttp2 wrapper for the http2 response.
class NgHttp2Response : public HttpResponse {
 public:
  explicit NgHttp2Response(
      const nghttp2::asio_http2::server::response& ng2_response)
      : ng2_response_(ng2_response),
        io_service_(ng2_response.io_service()),
        is_closed_(false) {
    ng2_response.on_close(
        std::bind(&NgHttp2Response::OnClose, this, std::placeholders::_1));
  }

  /// Sends the result to the caller.
  void Send() noexcept;

  /// The callback for when the request is completely closed
  std::function<void(uint32_t)> on_closed;

 private:
  /**
   * @brief Is called when the response is ending.
   *
   * @param error_code The error code for closing
   */
  void OnClose(uint32_t error_code) noexcept;
  /// A reference to the ng2 response object.
  const nghttp2::asio_http2::server::response& ng2_response_;
  /// A reference to the ng2 response io service.
  boost::asio::io_service& io_service_;
  /// Response mutex is used for synchronization between closing the connection
  /// and sending response.
  std::mutex response_mutex_;
  /// Indicates whether the response stream is closed.
  bool is_closed_;
};
}  // namespace google::scp::core
