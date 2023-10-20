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
using std::placeholders::_1;

namespace google::scp::core {
void NgHttp2Response::OnClose(OnCloseErrorCode close_error_code,
                              OnCloseCallback callback) noexcept {
  // While Onclose is running, ensure that there is no concurrent
  // io_service.post() or no concurrent sending response back on the connection.
  {
    std::unique_lock lock(on_close_mutex_);
    is_closed_ = true;
  }
  callback(close_error_code);
}

void NgHttp2Response::SetOnCloseCallback(const OnCloseCallback& callback) {
  ng2_response_.on_close(
      std::bind(&NgHttp2Response::OnClose, this, _1, callback));
}

void NgHttp2Response::Send() noexcept {
  // Before touching the ng2_response_ object reference to send response, we
  // must ensure that close hasn't been called before.
  std::unique_lock lock(on_close_mutex_);
  if (is_closed_) {
    return;
  }

  bool is_sensitive_header_value = false;
  header_map response_headers;
  for (const auto& [header, value] : *headers) {
    header_value ng_header_val{value, is_sensitive_header_value};
    response_headers.insert({header, ng_header_val});
  }
  try {
    ng2_response_.write_head(static_cast<int>(code), response_headers);
    if (body.length > 0) {
      ng2_response_.end(body.ToString());
    } else {
      ng2_response_.end("");
    }
  } catch (const boost::exception& ex) {
    // TODO: handle this
    std::string info = boost::diagnostic_information(ex);
    throw std::runtime_error(info);
  }
}

void NgHttp2Response::SubmitWorkOnIoService(
    std::function<void()> work) noexcept {
  // If the on_close is already executing or has already executed, do not
  // schedule the work as the ng2_response_ object may have been garbage
  // collected in the nghttp2 library. If the connection is closed, no work
  // (response to send) to be done.
  std::unique_lock lock(on_close_mutex_);
  if (is_closed_) {
    return;
  }
  ng2_response_.io_service().post(work);
}
}  // namespace google::scp::core
