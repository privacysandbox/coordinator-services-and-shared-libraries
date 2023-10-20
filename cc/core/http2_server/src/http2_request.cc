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

#include "http2_request.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "public/core/interface/execution_result.h"

#include "http2_utils.h"

using google::scp::core::http2_server::Http2Utils;
using std::bind;
using std::copy;
using std::make_pair;
using std::make_shared;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;

static constexpr size_t kMaxRequestBodySize = 1 * 1024 * 1024 * 1024;  // 100MB

namespace google::scp::core {

ExecutionResult NgHttp2Request::ReadUri() noexcept {
  handler_path = ng2_request_.uri().path;
  return SuccessExecutionResult();
}

ExecutionResult NgHttp2Request::ReadMethod() noexcept {
  // Parse the method
  if (ng2_request_.method() == "GET") {
    method = HttpMethod::GET;
    return SuccessExecutionResult();
  }
  if (ng2_request_.method() == "POST") {
    method = HttpMethod::POST;
    return SuccessExecutionResult();
  }

  method = HttpMethod::UNKNOWN;
  return FailureExecutionResult(errors::SC_HTTP2_SERVER_INVALID_METHOD);
}

ExecutionResult NgHttp2Request::ReadHeaders() noexcept {
  headers = make_shared<HttpHeaders>();
  for (const auto& header : ng2_request_.header()) {
    headers->insert({header.first, header.second.value});
  }
  return SuccessExecutionResult();
}

void NgHttp2Request::OnRequestBodyDataChunkReceived(
    const uint8_t* data, std::size_t length,
    const RequestBodyDataReceivedCallback& callback) noexcept {
  if (length == 0) {
    auto execution_result = SuccessExecutionResult();
    if (body.length < body.capacity) {
      execution_result =
          FailureExecutionResult(errors::SC_HTTP2_SERVER_PARTIAL_REQUEST_BODY);
    }
    callback(execution_result);
    return;
  }
  // Check if we are out of capacity. Avoiding overflow here.
  if (length > body.capacity || body.length > body.capacity - length) {
    auto execution_result =
        FailureExecutionResult(errors::SC_HTTP2_SERVER_PARTIAL_REQUEST_BODY);
    callback(execution_result);
    return;
  }
  // Otherwise, copy in data.
  copy(data, data + length, body.bytes->begin() + body.length);
  body.length += length;
}

ExecutionResult NgHttp2Request::UnwrapNgHttp2Request() noexcept {
  auto execution_result = ReadUri();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  execution_result = ReadMethod();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  execution_result = ReadHeaders();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  size_t content_length = 0;
  if (method != HttpMethod::GET) {
    execution_result = Http2Utils::ParseContentLength(headers, content_length);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    if (content_length > kMaxRequestBodySize) {
      return FailureExecutionResult(
          core::errors::SC_HTTP2_SERVER_INVALID_HEADER);
    }
  }
  body.bytes = make_shared<vector<Byte>>(content_length);
  body.length = 0;
  body.capacity = content_length;
  return SuccessExecutionResult();
}

void NgHttp2Request::SetOnRequestBodyDataReceivedCallback(
    const RequestBodyDataReceivedCallback& callback) {
  ng2_request_.on_data(bind(&NgHttp2Request::OnRequestBodyDataChunkReceived,
                            this, _1, _2, callback));
}

}  // namespace google::scp::core
