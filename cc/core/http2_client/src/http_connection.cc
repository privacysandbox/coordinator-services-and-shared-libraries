
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
#include "http_connection.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <nghttp2/asio_http2.h>
#include <nghttp2/asio_http2_client.h>

#include "absl/strings/str_cat.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/utils/src/http.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "http2_client.h"

using boost::asio::executor_work_guard;
using boost::asio::io_context;
using boost::asio::io_service;
using boost::asio::make_work_guard;
using boost::asio::post;
using boost::asio::ip::tcp;
using boost::asio::ssl::context;
using boost::posix_time::seconds;
using boost::system::error_code;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::core::utils::GetEscapedUriWithQuery;
using nghttp2::asio_http2::header_map;
using nghttp2::asio_http2::client::configure_tls_context;
using nghttp2::asio_http2::client::response;
using nghttp2::asio_http2::client::session;
using std::bind;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;

static constexpr char kContentLengthHeader[] = "content-length";
static constexpr char kHttp2Client[] = "Http2Client";
static constexpr char kHttpMethodGetTag[] = "GET";
static constexpr char kHttpMethodPostTag[] = "POST";

namespace google::scp::core {
HttpConnection::HttpConnection(
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const string& host, const string& service, bool is_https,
    TimeDuration http2_read_timeout_in_sec)
    : async_executor_(async_executor),
      host_(host),
      service_(service),
      is_https_(is_https),
      http2_read_timeout_in_sec_(http2_read_timeout_in_sec),
      tls_context_(context::sslv23),
      is_ready_(false),
      is_dropped_(false) {}

ExecutionResult HttpConnection::Init() noexcept {
  try {
    io_service_ = make_unique<io_service>();
    work_guard_ = make_unique<executor_work_guard<io_context::executor_type>>(
        make_work_guard(io_service_->get_executor()));

    tls_context_.set_default_verify_paths();
    error_code ec;
    configure_tls_context(ec, tls_context_);
    if (ec.failed()) {
      auto result =
          FailureExecutionResult(errors::SC_HTTP2_CLIENT_TLS_CTX_ERROR);
      SCP_ERROR(kHttp2Client, kZeroUuid, result,
                "Failed to initialize with tls ctx error %s.",
                ec.message().c_str());
      return result;
    }

    if (is_https_) {
      session_ =
          make_shared<session>(*io_service_, tls_context_, host_, service_);
    } else {
      session_ = make_shared<session>(*io_service_, host_, service_);
    }

    session_->read_timeout(seconds(http2_read_timeout_in_sec_));
    session_->on_connect(bind(&HttpConnection::OnConnectionCreated, this, _1));
    session_->on_error(bind(&HttpConnection::OnConnectionError, this));
    return SuccessExecutionResult();
  } catch (...) {
    auto result = FailureExecutionResult(
        errors::SC_HTTP2_CLIENT_CONNECTION_INITIALIZATION_FAILED);
    SCP_ERROR(kHttp2Client, kZeroUuid, result, "Failed to initialize.");
    return result;
  }

  SCP_INFO(kHttp2Client, kZeroUuid, "Initialized connection with ID: %p", this);
}

ExecutionResult HttpConnection::Run() noexcept {
  worker_ = make_shared<std::thread>([this]() {
    try {
      io_service_->run();
    } catch (...) {
      is_dropped_ = true;
      is_ready_ = false;
    }
  });
  return SuccessExecutionResult();
}

ExecutionResult HttpConnection::Stop() noexcept {
  if (session_) {
    // Post session_->shutdown in io_service to make sure only one thread invoke
    // the session.
    post(*io_service_, [this]() {
      session_->shutdown();
      SCP_INFO(kHttp2Client, kZeroUuid, "Session is being shutdown.");
    });
  }

  is_ready_ = false;

  try {
    work_guard_->reset();
    // Post io_service_->stop to make sure pervious tasks completed before
    // stop io_service_.
    post(*io_service_, [this]() {
      io_service_->stop();
      SCP_INFO(kHttp2Client, kZeroUuid, "IO service is stopping.");
    });

    if (worker_->joinable()) {
      worker_->join();
    }

    // Cancell all pending callbacks after stop io_service_.
    CancelPendingCallbacks();

    return SuccessExecutionResult();
  } catch (...) {
    auto result =
        FailureExecutionResult(errors::SC_HTTP2_CLIENT_CONNECTION_STOP_FAILED);
    SCP_ERROR(kHttp2Client, kZeroUuid, result, "Failed to stop.");
    return result;
  }
}

void HttpConnection::OnConnectionCreated(tcp::resolver::iterator) noexcept {
  post(*io_service_, [this]() mutable {
    SCP_INFO(kHttp2Client, kZeroUuid,
             "Connection %p for host %s is established.", this, host_.c_str());
    is_ready_ = true;
  });
}

void HttpConnection::OnConnectionError() noexcept {
  post(*io_service_, [this]() mutable {
    auto failure =
        FailureExecutionResult(errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED);
    SCP_ERROR(kHttp2Client, kZeroUuid, failure,
              "Connection %p for host %s got an error.", this, host_.c_str());

    is_ready_ = false;
    is_dropped_ = true;

    CancelPendingCallbacks();
  });
}

void HttpConnection::CancelPendingCallbacks() noexcept {
  vector<Uuid> keys;
  auto execution_result = pending_network_calls_.Keys(keys);
  if (!execution_result.Successful()) {
    SCP_ERROR(kHttp2Client, kZeroUuid, execution_result,
              "Cannot get the list of pending callbacks for the connection.");
    return;
  }

  for (auto key : keys) {
    AsyncContext<HttpRequest, HttpResponse> http_context;
    execution_result = pending_network_calls_.Find(key, http_context);

    if (!execution_result.Successful()) {
      SCP_ERROR(kHttp2Client, kZeroUuid, execution_result,
                "Cannot get the callback for the pending call connection.");
      continue;
    }

    // If Erase() failed, which means the context has being Finished.
    if (!pending_network_calls_.Erase(key).Successful()) {
      continue;
    }

    // The http_context should retry if the connection is dropped causing the
    // connection to be recycled.
    if (is_dropped_) {
      http_context.result =
          RetryExecutionResult(errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED);
    } else {
      http_context.result =
          FailureExecutionResult(errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED);
    }

    SCP_ERROR_CONTEXT(kHttp2Client, http_context, http_context.result,
                      "Pending callback context is dropped.");
    http_context.Finish();
  }
}

void HttpConnection::Reset() noexcept {
  is_ready_ = false;
  is_dropped_ = false;
  session_ = nullptr;
}

bool HttpConnection::IsDropped() noexcept {
  return is_dropped_.load();
}

bool HttpConnection::IsReady() noexcept {
  return is_ready_.load();
}

ExecutionResult HttpConnection::Execute(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  if (!is_ready_) {
    auto failure =
        RetryExecutionResult(errors::SC_HTTP2_CLIENT_NO_CONNECTION_ESTABLISHED);
    SCP_ERROR_CONTEXT(kHttp2Client, http_context, failure,
                      "The connection isn't ready.");
    return failure;
  }

  // This call needs to pass, otherwise there will be orphaned context when
  // connection drop happens.
  auto request_id = Uuid::GenerateUuid();
  auto pair = make_pair(request_id, http_context);
  auto execution_result = pending_network_calls_.Insert(pair, http_context);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  post(*io_service_, [this, http_context, request_id]() mutable {
    SendHttpRequest(request_id, http_context);
  });
  return SuccessExecutionResult();
}

void HttpConnection::SendHttpRequest(
    Uuid& request_id,
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  string method;
  if (http_context.request->method == HttpMethod::GET) {
    method = kHttpMethodGetTag;
  } else if (http_context.request->method == HttpMethod::POST) {
    method = kHttpMethodPostTag;
  } else {
    if (!pending_network_calls_.Erase(request_id).Successful()) {
      return;
    }

    http_context.result = FailureExecutionResult(
        errors::SC_HTTP2_CLIENT_HTTP_METHOD_NOT_SUPPORTED);
    SCP_ERROR_CONTEXT(kHttp2Client, http_context, http_context.result,
                      "Failed as request method not supported.");
    FinishContext(http_context.result, http_context, async_executor_);
    return;
  }

  // Copy headers
  header_map headers;
  if (http_context.request->headers) {
    for (const auto& [header, value] : *http_context.request->headers) {
      headers.insert({header, {value, false}});
    }
  }

  // TODO: handle large data, avoid copy
  string body;
  if (http_context.request->body.length > 0) {
    body = {http_context.request->body.bytes->begin(),
            http_context.request->body.bytes->end()};
  }

  // Erase the header if it is already present.
  headers.erase(kContentLengthHeader);
  headers.insert(
      {string(kContentLengthHeader), {std::to_string(body.length()), false}});

  // Erase the header if it is already present.
  headers.erase(kClientActivityIdHeader);
  headers.insert({string(kClientActivityIdHeader),
                  {ToString(http_context.activity_id), false}});

  auto uri = GetEscapedUriWithQuery(*http_context.request);
  if (!uri.Successful()) {
    if (!pending_network_calls_.Erase(request_id).Successful()) {
      return;
    }

    SCP_ERROR_CONTEXT(kHttp2Client, http_context, uri.result(),
                      "Failed escaping URI.");
    FinishContext(uri.result(), http_context, async_executor_);
    return;
  }

  error_code ec;
  auto http_request = session_->submit(ec, method, uri.value(), body, headers);
  if (ec) {
    if (!pending_network_calls_.Erase(request_id).Successful()) {
      return;
    }

    http_context.result = RetryExecutionResult(
        errors::SC_HTTP2_CLIENT_FAILED_TO_ISSUE_HTTP_REQUEST);
    SCP_ERROR_CONTEXT(kHttp2Client, http_context, http_context.result,
                      "Http request failed for the client with error code %s!",
                      ec.message().c_str());

    FinishContext(http_context.result, http_context, async_executor_);

    OnConnectionError();
    return;
  }

  http_context.response = make_shared<HttpResponse>();
  http_request->on_response(
      bind(&HttpConnection::OnResponseCallback, this, http_context, _1));
  http_request->on_close(bind(&HttpConnection::OnRequestResponseClosed, this,
                              request_id, http_context, _1));
}

void HttpConnection::OnRequestResponseClosed(
    Uuid& request_id, AsyncContext<HttpRequest, HttpResponse>& http_context,
    uint32_t error_code) noexcept {
  if (!pending_network_calls_.Erase(request_id).Successful()) {
    return;
  }

  auto result =
      ConvertHttpStatusCodeToExecutionResult(http_context.response->code);

  // `!error_code` means no error during on_close.
  if (!error_code) {
    http_context.result = result;
    SCP_DEBUG_CONTEXT(kHttp2Client, http_context,
                      "Response has status code: %d",
                      static_cast<int>(http_context.response->code));
  } else {
    // `!result.Successful() && result != FailureExecutionResult(SC_UNKNOWN)`
    // means http_context got failure response code.
    if (!result.Successful() && result != FailureExecutionResult(SC_UNKNOWN)) {
      http_context.result = result;
    } else {
      http_context.result = RetryExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_REQUEST_CLOSE_ERROR);
    }
    SCP_DEBUG_CONTEXT(
        kHttp2Client, http_context,
        "Http request failed request on_close with error code %s, "
        "and the context response has status code: %d",
        to_string(error_code).c_str(),
        static_cast<int>(http_context.response->code));
  }

  FinishContext(http_context.result, http_context, async_executor_);
}

void HttpConnection::OnResponseCallback(
    AsyncContext<HttpRequest, HttpResponse>& http_context,
    const response& http_response) noexcept {
  http_context.response->headers = make_shared<HttpHeaders>();
  http_context.response->code =
      static_cast<errors::HttpStatusCode>(http_response.status_code());

  if (http_response.status_code() !=
      static_cast<int>(errors::HttpStatusCode::OK)) {
    std::string headers_string;
    for (auto header : http_response.header()) {
      headers_string += header.first + " " + header.second.value + "|";
    }
    SCP_DEBUG_CONTEXT(kHttp2Client, http_context,
                      "Http response is not OK. Endpoint: %s, status code: %d, "
                      "Headers: %s",
                      http_context.request->path->c_str(),
                      http_response.status_code(), headers_string.c_str());
  }

  for (const auto& [header, value] : http_response.header()) {
    http_context.response->headers->insert({header, value.value});
  }

  if (http_response.content_length() >= 0) {
    http_context.response->body.bytes = make_shared<vector<Byte>>();
    http_context.response->body.bytes->reserve(http_response.content_length());
    http_context.response->body.capacity = http_response.content_length();
  }

  http_response.on_data(bind(&HttpConnection::OnResponseBodyCallback, this,
                             http_context, _1, _2));
}

void HttpConnection::OnResponseBodyCallback(
    AsyncContext<HttpRequest, HttpResponse>& http_context, const uint8_t* data,
    size_t chunk_length) noexcept {
  auto is_last_chunk = chunk_length == 0UL;
  if (!is_last_chunk) {
    auto& body = http_context.response->body;
    auto& body_buffer = *http_context.response->body.bytes;
    if (body_buffer.capacity() < body_buffer.size() + chunk_length) {
      body_buffer.reserve(body_buffer.size() + chunk_length);
      body.capacity = body_buffer.size() + chunk_length;
    }
    std::copy(data, data + chunk_length, std::back_inserter(body_buffer));
    http_context.response->body.length += chunk_length;
  }
}

ExecutionResult HttpConnection::ConvertHttpStatusCodeToExecutionResult(
    const errors::HttpStatusCode status_code) noexcept {
  switch (status_code) {
    case errors::HttpStatusCode::OK:
    case errors::HttpStatusCode::CREATED:
    case errors::HttpStatusCode::ACCEPTED:
    case errors::HttpStatusCode::NO_CONTENT:
    case errors::HttpStatusCode::PARTIAL_CONTENT:
      return SuccessExecutionResult();
    case errors::HttpStatusCode::MULTIPLE_CHOICES:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_MULTIPLE_CHOICES);
    case errors::HttpStatusCode::MOVED_PERMANENTLY:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_MOVED_PERMANENTLY);
    case errors::HttpStatusCode::FOUND:
      return FailureExecutionResult(errors::SC_HTTP2_CLIENT_HTTP_STATUS_FOUND);
    case errors::HttpStatusCode::SEE_OTHER:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_SEE_OTHER);
    case errors::HttpStatusCode::NOT_MODIFIED:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_NOT_MODIFIED);
    case errors::HttpStatusCode::TEMPORARY_REDIRECT:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_TEMPORARY_REDIRECT);
    case errors::HttpStatusCode::PERMANENT_REDIRECT:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_PERMANENT_REDIRECT);
    case errors::HttpStatusCode::BAD_REQUEST:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_BAD_REQUEST);
    case errors::HttpStatusCode::UNAUTHORIZED:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_UNAUTHORIZED);
    case errors::HttpStatusCode::FORBIDDEN:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_FORBIDDEN);
    case errors::HttpStatusCode::NOT_FOUND:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND);
    case errors::HttpStatusCode::METHOD_NOT_ALLOWED:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_METHOD_NOT_ALLOWED);
    case errors::HttpStatusCode::REQUEST_TIMEOUT:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_TIMEOUT);
    case errors::HttpStatusCode::CONFLICT:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT);
    case errors::HttpStatusCode::GONE:
      return FailureExecutionResult(errors::SC_HTTP2_CLIENT_HTTP_STATUS_GONE);
    case errors::HttpStatusCode::LENGTH_REQUIRED:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_LENGTH_REQUIRED);
    case errors::HttpStatusCode::PRECONDITION_FAILED:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_PRECONDITION_FAILED);
    case errors::HttpStatusCode::REQUEST_ENTITY_TOO_LARGE:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE);
    case errors::HttpStatusCode::REQUEST_URI_TOO_LONG:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_URI_TOO_LONG);
    case errors::HttpStatusCode::UNSUPPORTED_MEDIA_TYPE:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);
    case errors::HttpStatusCode::REQUEST_RANGE_NOT_SATISFIABLE:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_RANGE_NOT_SATISFIABLE);
    case errors::HttpStatusCode::MISDIRECTED_REQUEST:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_MISDIRECTED_REQUEST);
    case errors::HttpStatusCode::TOO_MANY_REQUESTS:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_TOO_MANY_REQUESTS);
    case errors::HttpStatusCode::INTERNAL_SERVER_ERROR:
      return RetryExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_INTERNAL_SERVER_ERROR);
    case errors::HttpStatusCode::NOT_IMPLEMENTED:
      return RetryExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_NOT_IMPLEMENTED);
    case errors::HttpStatusCode::BAD_GATEWAY:
      return RetryExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_BAD_GATEWAY);
    case errors::HttpStatusCode::SERVICE_UNAVAILABLE:
      return RetryExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_SERVICE_UNAVAILABLE);
    case errors::HttpStatusCode::GATEWAY_TIMEOUT:
      return RetryExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_GATEWAY_TIMEOUT);
    case errors::HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED:
      return RetryExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED);
    case errors::HttpStatusCode::UNKNOWN:
      return FailureExecutionResult(SC_UNKNOWN);
    default:
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_HTTP_REQUEST_RESPONSE_STATUS_UNKNOWN);
  }
}
}  // namespace google::scp::core
