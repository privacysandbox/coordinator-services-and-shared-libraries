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
#include "http2_response.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/exception/diagnostic_information.hpp>

#include "public/core/interface/execution_result.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using nghttp2::asio_http2::header_map;
using nghttp2::asio_http2::header_value;
using std::make_pair;
using std::string;
using std::vector;

namespace google::scp::core {
void NgHttp2Response::OnClose(uint32_t error_code) noexcept {
  response_mutex_.lock();
  is_closed_ = true;
  response_mutex_.unlock();

  if (on_closed) {
    on_closed(error_code);
  }
}

void NgHttp2Response::Send() noexcept {
  io_service_.post([this]() mutable {
    response_mutex_.lock();
    if (is_closed_) {
      response_mutex_.unlock();
      return;
    }
    header_map response_headers;
    for (const auto& [header, value] : *headers) {
      header_value ng_header_val{value, false};
      response_headers.insert({header, ng_header_val});
    }
    try {
      ng2_response_.write_head(static_cast<int>(code), response_headers);
      if (body.length > 0) {
        ng2_response_.end(string(body.bytes->data(), body.bytes->size()));
      } else {
        ng2_response_.end("");
      }
    } catch (const boost::exception& ex) {
      // TODO: handle this
      std::string info = boost::diagnostic_information(ex);

      throw std::runtime_error(info);
    }
    response_mutex_.unlock();
  });
}
}  // namespace google::scp::core
