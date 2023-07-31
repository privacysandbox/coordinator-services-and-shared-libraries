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

#include <algorithm>
#include <memory>
#include <string>

#include "core/interface/http_server_interface.h"

#include "error_codes.h"

namespace google::scp::core::http2_server {
class Http2Utils {
 public:
  /**
   * @brief Parses the content-length value in the http headers.
   *
   * @param headers The map of all the headers.
   * @param content_length The content length to be set if the operation was
   * successful.
   * @return ExecutionResult The execution result of the operation.
   */
  static ExecutionResult ParseContentLength(
      // TODO: In HTTP2, content-length header is not required anymore,
      // see https://datatracker.ietf.org/doc/html/rfc7540#section-8.1.2.6
      // We should not rely on this header at all.
      std::shared_ptr<HttpHeaders>& headers, size_t& content_length) noexcept {
    auto header_iter = headers->find("content-length");
    if (header_iter == headers->end()) {
      return FailureExecutionResult(core::errors::SC_HTTP2_SERVER_BAD_REQUEST);
    }
    const auto& content_length_val = header_iter->second;

    if (!IsNumber(content_length_val)) {
      return FailureExecutionResult(
          core::errors::SC_HTTP2_SERVER_INVALID_HEADER);
    }

    try {
      content_length = strtoul(content_length_val.c_str(), NULL, 0);
    } catch (...) {
      return FailureExecutionResult(
          core::errors::SC_HTTP2_SERVER_INVALID_HEADER);
    }

    return SuccessExecutionResult();
  }

 private:
  /**
   * @brief Checks that a string is number.
   *
   * @param input The input string.
   * @return true If the value is a number.
   * @return false If the value is not a number.
   */
  static bool IsNumber(const std::string& input) {
    return !input.empty() && std::all_of(input.begin(), input.end(), ::isdigit);
  }
};
}  // namespace google::scp::core::http2_server
