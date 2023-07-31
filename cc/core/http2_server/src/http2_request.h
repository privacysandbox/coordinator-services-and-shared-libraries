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
#include <string>
#include <thread>

#include <nghttp2/asio_http2_server.h>

#include "cc/core/interface/http_server_interface.h"
#include "core/common/uuid/src/uuid.h"

namespace google::scp::core {

/// Nghttp2 wrapper for the http2 request.
class NgHttp2Request : public HttpRequest {
 public:
  explicit NgHttp2Request(
      const nghttp2::asio_http2::server::request& ng2_request)
      : id(common::Uuid::GenerateUuid()), ng2_request_(ng2_request) {}

  /**
   * @brief Unwraps the ngHttp2 request and update the current object.
   *
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult UnwrapNgHttp2Request() noexcept;

  /// The auto-generated id of the request.
  const common::Uuid id;

  /// The callback for when the request body is completely received.
  std::function<void(ExecutionResult&)> on_request_body_received;

 protected:
  /**
   * @brief Reads the Uri from the ngHttp2Request object.
   *
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult ReadUri() noexcept;

  /**
   * @brief Reads the http method from the ngHttp2Request object.
   *
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult ReadMethod() noexcept;

  /**
   * @brief Reads the http headers from the ngHttp2Request object.
   *
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult ReadHeaders() noexcept;

  /**
   * @brief Is called when there is a body on the request.
   *
   * @param bytes The bytes received.
   * @param length The length of the bytes received.
   */
  void OnBodyDataReceived(const uint8_t* bytes, std::size_t length) noexcept;

 private:
  /// A ref to the original ng2_request.
  const nghttp2::asio_http2::server::request& ng2_request_;
};

}  // namespace google::scp::core
