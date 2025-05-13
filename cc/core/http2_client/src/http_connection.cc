
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
#include "cc/core/http2_client/src/http_connection.h"

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

#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/http2_client/src/error_codes.h"
#include "cc/core/http2_client/src/http_client_def.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/core/utils/src/http.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

namespace privacy_sandbox::pbs_common {
using ::boost::asio::executor_work_guard;
using ::boost::asio::io_context;
using ::boost::asio::io_service;
using ::boost::asio::make_work_guard;
using ::boost::asio::post;
using ::boost::asio::ip::tcp;
using ::boost::asio::ssl::context;
using ::boost::posix_time::seconds;
using ::boost::system::error_code;
using ::nghttp2::asio_http2::header_map;
using ::nghttp2::asio_http2::client::configure_tls_context;
using ::nghttp2::asio_http2::client::response;
using ::nghttp2::asio_http2::client::session;
using ::opentelemetry::sdk::resource::SemanticConventions::
    kHttpResponseStatusCode;
using ::opentelemetry::sdk::resource::SemanticConventions::kServerAddress;
using ::opentelemetry::sdk::resource::SemanticConventions::kServerPort;
using ::opentelemetry::sdk::resource::SemanticConventions::kUrlScheme;

static constexpr char kContentLengthHeader[] = "content-length";
static constexpr char kHttp2Client[] = "Http2Client";
static constexpr char kHttpMethodGetTag[] = "GET";
static constexpr char kHttpMethodPostTag[] = "POST";

HttpConnection::HttpConnection(
    const std::shared_ptr<AsyncExecutorInterface>& async_executor,
    const std::string& host, const std::string& service, bool is_https,
    absl::Nullable<MetricRouter*> metric_router,
    TimeDuration http2_read_timeout_in_sec)
    : async_executor_(async_executor),
      host_(host),
      service_(service),
      is_https_(is_https),
      http2_read_timeout_in_sec_(http2_read_timeout_in_sec),
      tls_context_(context::sslv23),
      is_ready_(false),
      is_dropped_(false),
      metric_router_(metric_router) {}

ExecutionResult HttpConnection::Init() noexcept {
  try {
    io_service_ = std::make_unique<io_service>();
    work_guard_ =
        std::make_unique<executor_work_guard<io_context::executor_type>>(
            make_work_guard(io_service_->get_executor()));

    tls_context_.set_default_verify_paths();
    error_code ec;
    configure_tls_context(ec, tls_context_);
    if (ec.failed()) {
      auto result = FailureExecutionResult(SC_HTTP2_CLIENT_TLS_CTX_ERROR);
      SCP_ERROR(kHttp2Client, kZeroUuid, result,
                absl::StrFormat("Failed to initialize with tls ctx error %s.",
                                ec.message().c_str()));
      return result;
    }

    RETURN_IF_FAILURE(MetricInit());

    if (is_https_) {
      session_ = std::make_shared<session>(*io_service_, tls_context_, host_,
                                           service_);
    } else {
      session_ = std::make_shared<session>(*io_service_, host_, service_);
    }

    session_->read_timeout(seconds(http2_read_timeout_in_sec_));
    session_->on_connect(std::bind(&HttpConnection::OnConnectionCreated, this,
                                   std::placeholders::_1));
    session_->on_error(std::bind(&HttpConnection::OnConnectionError, this));
    return SuccessExecutionResult();
  } catch (...) {
    auto result = FailureExecutionResult(
        SC_HTTP2_CLIENT_CONNECTION_INITIALIZATION_FAILED);
    SCP_ERROR(kHttp2Client, kZeroUuid, result, "Failed to initialize.");
    return result;
  }

  SCP_INFO(kHttp2Client, kZeroUuid,
           absl::StrFormat("Initialized connection with ID: %p", this));
}

ExecutionResult HttpConnection::Run() noexcept {
  worker_ = std::make_shared<std::thread>([this]() {
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
  RecordClientConnectionDuration();

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
        FailureExecutionResult(SC_HTTP2_CLIENT_CONNECTION_STOP_FAILED);
    SCP_ERROR(kHttp2Client, kZeroUuid, result, "Failed to stop.");
    return result;
  }
}

ExecutionResult HttpConnection::MetricInit() noexcept {
  if (!metric_router_) {
    return SuccessExecutionResult();
  }

  // Otel metrics setup.
  meter_ = metric_router_->GetOrCreateMeter(kHttpConnectionMeter);

  // Client-Server Latency View Setup.
  static std::vector<double> kClientServerLatencyBoundaries = {
      0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25,
      0.5,   0.75, 1,     2.5,  5,     7.5, 10};

  metric_router_->CreateViewForInstrument(
      /*meter_name=*/kHttpConnectionMeter,
      /*instrument_name=*/kClientServerLatencyMetric,
      /*instrument_type=*/
      opentelemetry::sdk::metrics::InstrumentType::kHistogram,
      /*aggregation_type=*/
      opentelemetry::sdk::metrics::AggregationType::kHistogram,
      /*boundaries=*/kClientServerLatencyBoundaries,
      /*version=*/"", /*schema=*/"",
      /*view_description=*/"Client Server Latency histogram",
      /*unit=*/kSecondUnit);

  // Client Request Duration View Setup.
  static std::vector<double> kClientRequestDurationBoundaries = {
      0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25,
      0.5,   0.75, 1,     2.5,  5,     7.5, 10};

  metric_router_->CreateViewForInstrument(
      /*meter_name=*/kHttpConnectionMeter,
      /*instrument_name=*/kClientRequestDurationMetric,
      /*instrument_type=*/
      opentelemetry::sdk::metrics::InstrumentType::kHistogram,
      /*aggregation_type=*/
      opentelemetry::sdk::metrics::AggregationType::kHistogram,
      /*boundaries=*/kClientRequestDurationBoundaries,
      /*version=*/"", /*schema=*/"",
      /*view_description=*/"Client Request Duration histogram",
      /*unit=*/kSecondUnit);

  // Client-Connection Duration View Setup.
  static std::vector<double> kClientConnectionDurationBoundaries = {
      0.1, 0.5, 1, 2, 5, 10, 20, 30, 60};

  metric_router_->CreateViewForInstrument(
      /*meter_name=*/kHttpConnectionMeter,
      /*instrument_name=*/kClientConnectionDurationMetric,
      /*instrument_type=*/
      opentelemetry::sdk::metrics::InstrumentType::kHistogram,
      /*aggregation_type=*/
      opentelemetry::sdk::metrics::AggregationType::kHistogram,
      /*boundaries=*/kClientConnectionDurationBoundaries,
      /*version=*/"", /*schema=*/"",
      /*view_description=*/"Connection duration histogram",
      /*unit=*/kSecondUnit);

  client_connect_error_counter_ =
      std::static_pointer_cast<opentelemetry::metrics::Counter<uint64_t>>(
          metric_router_->GetOrCreateSyncInstrument(
              kClientConnectErrorsMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateUInt64Counter(
                    kClientConnectErrorsMetric,
                    "Total number of client connect errors");
              }));

  client_server_latency_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<double>>(
          metric_router_->GetOrCreateSyncInstrument(
              kClientServerLatencyMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateDoubleHistogram(
                    kClientServerLatencyMetric,
                    "Client-Server latency in seconds", kSecondUnit);
              }));

  client_request_duration_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<double>>(
          metric_router_->GetOrCreateSyncInstrument(
              kClientRequestDurationMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateDoubleHistogram(
                    kClientRequestDurationMetric,
                    "Client request duration in seconds", kSecondUnit);
              }));

  client_request_body_size_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<uint64_t>>(
          metric_router_->GetOrCreateSyncInstrument(
              kClientRequestBodySizeMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateUInt64Histogram(
                    kClientRequestBodySizeMetric,
                    "Client request body size in Bytes - uncompressed",
                    kByteUnit);
              }));

  client_response_body_size_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<uint64_t>>(
          metric_router_->GetOrCreateSyncInstrument(
              kClientResponseBodySizeMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateUInt64Histogram(
                    kClientResponseBodySizeMetric,
                    "Client response body size in Bytes - uncompressed",
                    kByteUnit);
              }));

  client_connection_duration_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<double>>(
          metric_router_->GetOrCreateSyncInstrument(
              kClientConnectionDurationMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateDoubleHistogram(
                    kClientConnectionDurationMetric,
                    "Client connection duration in seconds", kSecondUnit);
              }));

  return SuccessExecutionResult();
}

void HttpConnection::OnConnectionCreated(tcp::resolver::iterator) noexcept {
  post(*io_service_, [this]() mutable {
    SCP_INFO(kHttp2Client, kZeroUuid,
             absl::StrFormat("Connection %p for host %s is established.", this,
                             host_.c_str()));
    is_ready_ = true;
    connection_creation_time_ = std::chrono::steady_clock::now();
  });
}

void HttpConnection::OnConnectionError() noexcept {
  post(*io_service_, [this]() mutable {
    auto failure = FailureExecutionResult(SC_HTTP2_CLIENT_CONNECTION_DROPPED);
    SCP_ERROR(kHttp2Client, kZeroUuid, failure,
              absl::StrFormat("Connection %p for host %s got an error.", this,
                              host_.c_str()));

    IncrementClientConnectError();

    is_ready_ = false;
    is_dropped_ = true;

    RecordClientConnectionDuration();

    CancelPendingCallbacks();
  });
}

void HttpConnection::CancelPendingCallbacks() noexcept {
  std::vector<Uuid> keys;
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
          RetryExecutionResult(SC_HTTP2_CLIENT_CONNECTION_DROPPED);
    } else {
      http_context.result =
          FailureExecutionResult(SC_HTTP2_CLIENT_CONNECTION_DROPPED);
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

size_t HttpConnection::ActiveClientRequestsSize() noexcept {
  return pending_network_calls_.Size();
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
        RetryExecutionResult(SC_HTTP2_CLIENT_NO_CONNECTION_ESTABLISHED);
    SCP_ERROR_CONTEXT(kHttp2Client, http_context, failure,
                      "The connection isn't ready.");
    return failure;
  }

  // This call needs to pass, otherwise there will be orphaned context when
  // connection drop happens.
  auto request_id = Uuid::GenerateUuid();
  auto pair = std::make_pair(request_id, http_context);
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
  std::string method;
  if (http_context.request->method == HttpMethod::GET) {
    method = kHttpMethodGetTag;
  } else if (http_context.request->method == HttpMethod::POST) {
    method = kHttpMethodPostTag;
  } else {
    if (!pending_network_calls_.Erase(request_id).Successful()) {
      return;
    }

    http_context.result =
        FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_METHOD_NOT_SUPPORTED);
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
  std::string body;
  if (http_context.request->body.length > 0) {
    body = {http_context.request->body.bytes->begin(),
            http_context.request->body.bytes->end()};
  }

  RecordClientRequestBodySize(http_context);

  // Erase the header if it is already present.
  headers.erase(kContentLengthHeader);
  headers.insert({std::string(kContentLengthHeader),
                  {std::to_string(body.length()), false}});

  // Erase the header if it is already present.
  headers.erase(kClientActivityIdHeader);
  headers.insert({std::string(kClientActivityIdHeader),
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
  std::chrono::time_point<std::chrono::steady_clock> submit_request_time =
      std::chrono::steady_clock::now();
  auto http_request = session_->submit(ec, method, uri.value(), body, headers);
  if (ec) {
    if (!pending_network_calls_.Erase(request_id).Successful()) {
      return;
    }

    http_context.result =
        RetryExecutionResult(SC_HTTP2_CLIENT_FAILED_TO_ISSUE_HTTP_REQUEST);
    SCP_ERROR_CONTEXT(
        kHttp2Client, http_context, http_context.result,
        absl::StrFormat(
            "Http request failed for the client with error code %s!",
            ec.message().c_str()));

    FinishContext(http_context.result, http_context, async_executor_);

    OnConnectionError();
    return;
  }

  http_context.response = std::make_shared<HttpResponse>();
  http_request->on_response(bind(&HttpConnection::OnResponseCallback, this,
                                 http_context, std::placeholders::_1,
                                 submit_request_time));
  http_request->on_close(bind(&HttpConnection::OnRequestResponseClosed, this,
                              request_id, http_context, std::placeholders::_1,
                              submit_request_time));
}

void HttpConnection::OnRequestResponseClosed(
    Uuid& request_id, AsyncContext<HttpRequest, HttpResponse>& http_context,
    uint32_t error_code,
    std::chrono::time_point<std::chrono::steady_clock>
        submit_request_time) noexcept {
  if (!pending_network_calls_.Erase(request_id).Successful()) {
    return;
  }

  auto result =
      ConvertHttpStatusCodeToExecutionResult(http_context.response->code);

  RecordClientResponseBodySize(http_context);
  RecordClientRequestDuration(http_context, submit_request_time);

  // `!error_code` means no error during on_close.
  if (!error_code) {
    http_context.result = result;
    SCP_DEBUG_CONTEXT(
        kHttp2Client, http_context,
        absl::StrFormat("Response has status code: %d",
                        static_cast<int>(http_context.response->code)));
  } else {
    // `!result.Successful() && result != FailureExecutionResult(SC_UNKNOWN)`
    // means http_context got failure response code.
    if (!result.Successful() && result != FailureExecutionResult(SC_UNKNOWN)) {
      http_context.result = result;
    } else {
      http_context.result =
          RetryExecutionResult(SC_HTTP2_CLIENT_HTTP_REQUEST_CLOSE_ERROR);
    }
    SCP_DEBUG_CONTEXT(
        kHttp2Client, http_context,
        absl::StrFormat(
            "Http request failed request on_close with error code %s, "
            "and the context response has status code: %d",
            std::to_string(error_code).c_str(),
            static_cast<int>(http_context.response->code)));
  }

  FinishContext(http_context.result, http_context, async_executor_);
}

void HttpConnection::OnResponseCallback(
    AsyncContext<HttpRequest, HttpResponse>& http_context,
    const response& http_response,
    std::chrono::time_point<std::chrono::steady_clock>
        submit_request_time) noexcept {
  http_context.response->headers = std::make_shared<HttpHeaders>();
  http_context.response->code =
      static_cast<HttpStatusCode>(http_response.status_code());
  RecordClientServerLatency(http_context, submit_request_time);

  if (http_response.status_code() != static_cast<int>(HttpStatusCode::OK)) {
    std::string headers_string;
    for (auto header : http_response.header()) {
      headers_string += header.first + " " + header.second.value + "|";
    }
    SCP_DEBUG_CONTEXT(
        kHttp2Client, http_context,
        absl::StrFormat(
            "Http response is not OK. Endpoint: %s, status code: %d, "
            "Headers: %s",
            http_context.request->path->c_str(), http_response.status_code(),
            headers_string.c_str()));
  }

  for (const auto& [header, value] : http_response.header()) {
    http_context.response->headers->insert({header, value.value});
  }

  if (http_response.content_length() >= 0) {
    http_context.response->body.bytes = std::make_shared<std::vector<Byte>>();
    http_context.response->body.bytes->reserve(http_response.content_length());
    http_context.response->body.capacity = http_response.content_length();
  }

  http_response.on_data(bind(&HttpConnection::OnResponseBodyCallback, this,
                             http_context, std::placeholders::_1,
                             std::placeholders::_2));
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
    const HttpStatusCode status_code) noexcept {
  switch (status_code) {
    case HttpStatusCode::OK:
    case HttpStatusCode::CREATED:
    case HttpStatusCode::ACCEPTED:
    case HttpStatusCode::NO_CONTENT:
    case HttpStatusCode::PARTIAL_CONTENT:
      return SuccessExecutionResult();
    case HttpStatusCode::MULTIPLE_CHOICES:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_MULTIPLE_CHOICES);
    case HttpStatusCode::MOVED_PERMANENTLY:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_MOVED_PERMANENTLY);
    case HttpStatusCode::FOUND:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_FOUND);
    case HttpStatusCode::SEE_OTHER:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_SEE_OTHER);
    case HttpStatusCode::NOT_MODIFIED:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_NOT_MODIFIED);
    case HttpStatusCode::TEMPORARY_REDIRECT:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_TEMPORARY_REDIRECT);
    case HttpStatusCode::PERMANENT_REDIRECT:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_PERMANENT_REDIRECT);
    case HttpStatusCode::BAD_REQUEST:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_BAD_REQUEST);
    case HttpStatusCode::UNAUTHORIZED:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_UNAUTHORIZED);
    case HttpStatusCode::FORBIDDEN:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_FORBIDDEN);
    case HttpStatusCode::NOT_FOUND:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND);
    case HttpStatusCode::METHOD_NOT_ALLOWED:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_METHOD_NOT_ALLOWED);
    case HttpStatusCode::REQUEST_TIMEOUT:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_TIMEOUT);
    case HttpStatusCode::CONFLICT:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT);
    case HttpStatusCode::GONE:
      return FailureExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_GONE);
    case HttpStatusCode::LENGTH_REQUIRED:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_LENGTH_REQUIRED);
    case HttpStatusCode::PRECONDITION_FAILED:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_PRECONDITION_FAILED);
    case HttpStatusCode::REQUEST_ENTITY_TOO_LARGE:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE);
    case HttpStatusCode::REQUEST_URI_TOO_LONG:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_URI_TOO_LONG);
    case HttpStatusCode::UNSUPPORTED_MEDIA_TYPE:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);
    case HttpStatusCode::REQUEST_RANGE_NOT_SATISFIABLE:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_RANGE_NOT_SATISFIABLE);
    case HttpStatusCode::MISDIRECTED_REQUEST:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_MISDIRECTED_REQUEST);
    case HttpStatusCode::TOO_MANY_REQUESTS:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_TOO_MANY_REQUESTS);
    case HttpStatusCode::INTERNAL_SERVER_ERROR:
      return RetryExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_INTERNAL_SERVER_ERROR);
    case HttpStatusCode::NOT_IMPLEMENTED:
      return RetryExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_NOT_IMPLEMENTED);
    case HttpStatusCode::BAD_GATEWAY:
      return RetryExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_BAD_GATEWAY);
    case HttpStatusCode::SERVICE_UNAVAILABLE:
      return RetryExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_SERVICE_UNAVAILABLE);
    case HttpStatusCode::GATEWAY_TIMEOUT:
      return RetryExecutionResult(SC_HTTP2_CLIENT_HTTP_STATUS_GATEWAY_TIMEOUT);
    case HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED:
      return RetryExecutionResult(
          SC_HTTP2_CLIENT_HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED);
    case HttpStatusCode::UNKNOWN:
      return FailureExecutionResult(SC_UNKNOWN);
    default:
      return FailureExecutionResult(
          SC_HTTP2_CLIENT_HTTP_REQUEST_RESPONSE_STATUS_UNKNOWN);
  }
}

void HttpConnection::IncrementClientConnectError() {
  if (!client_connect_error_counter_) {
    return;
  }
  absl::flat_hash_map<absl::string_view, std::string> labels = {
      {kServerAddress, host_},
      {kServerPort, service_},
      {kUrlScheme, is_https_ ? "https" : "http"},
  };

  client_connect_error_counter_->Add(1, labels);
}

void HttpConnection::RecordClientServerLatency(
    const AsyncContext<HttpRequest, HttpResponse>& http_context,
    std::chrono::time_point<std::chrono::steady_clock> submit_request_time) {
  if (!client_server_latency_) {
    return;
  }
  std::chrono::duration<double> latency =
      std::chrono::steady_clock::now() - submit_request_time;
  double latency_s = latency.count();

  auto result =
      ConvertHttpStatusCodeToExecutionResult(http_context.response->code);

  absl::flat_hash_map<absl::string_view, std::string> labels = {
      {kServerAddress, host_},
      {kServerPort, service_},
      {kUrlScheme, is_https_ ? "https" : "http"},
      {kHttpResponseStatusCode,
       std::to_string(static_cast<int>(http_context.response->code))},
      {kClientReturnCodeLabel, std::to_string(result.status_code)},
      {kPbsClaimedIdentityLabel,
       GetClaimedIdentityOrUnknownValue(http_context)}};

  opentelemetry::context::Context context;
  client_server_latency_->Record(latency_s, labels, context);
}

void HttpConnection::RecordClientRequestDuration(
    const AsyncContext<HttpRequest, HttpResponse>& http_context,
    std::chrono::time_point<std::chrono::steady_clock> submit_request_time) {
  if (!client_request_duration_) {
    return;
  }
  std::chrono::duration<double> duration =
      std::chrono::steady_clock::now() - submit_request_time;
  double duration_s = duration / std::chrono::seconds(1);

  auto result =
      ConvertHttpStatusCodeToExecutionResult(http_context.response->code);

  absl::flat_hash_map<absl::string_view, std::string> labels = {
      {kServerAddress, host_},
      {kServerPort, service_},
      {kUrlScheme, is_https_ ? "https" : "http"},
      {kHttpResponseStatusCode,
       std::to_string(static_cast<int>(http_context.response->code))},
      {kClientReturnCodeLabel, std::to_string(result.status_code)},
      {kPbsClaimedIdentityLabel,
       GetClaimedIdentityOrUnknownValue(http_context)}};

  opentelemetry::context::Context context;
  client_request_duration_->Record(duration_s, labels, context);
}

void HttpConnection::RecordClientConnectionDuration() {
  if (!client_connection_duration_) {
    return;
  }
  std::chrono::duration<double> latency =
      std::chrono::steady_clock::now() - connection_creation_time_;
  double latency_s = latency / std::chrono::seconds(1);

  absl::flat_hash_map<absl::string_view, std::string> labels = {
      {kServerAddress, host_},
      {kServerPort, service_},
      {kUrlScheme, is_https_ ? "https" : "http"}};
  opentelemetry::context::Context context;
  client_connection_duration_->Record(latency_s, labels, context);
}

void HttpConnection::RecordClientRequestBodySize(
    const AsyncContext<HttpRequest, HttpResponse>& http_context) {
  if (!client_request_body_size_) {
    return;
  }
  absl::flat_hash_map<absl::string_view, std::string> labels = {
      {kServerAddress, host_},
      {kServerPort, service_},
      {kUrlScheme, is_https_ ? "https" : "http"},
      {kPbsClaimedIdentityLabel,
       GetClaimedIdentityOrUnknownValue(http_context)}};

  opentelemetry::context::Context context;
  client_request_body_size_->Record(http_context.request->body.length, labels,
                                    context);
}

void HttpConnection::RecordClientResponseBodySize(
    const AsyncContext<HttpRequest, HttpResponse>& http_context) {
  if (!client_response_body_size_) {
    return;
  }
  auto result =
      ConvertHttpStatusCodeToExecutionResult(http_context.response->code);

  absl::flat_hash_map<absl::string_view, std::string> labels = {
      {kServerAddress, host_},
      {kServerPort, service_},
      {kUrlScheme, is_https_ ? "https" : "http"},
      {kHttpResponseStatusCode,
       std::to_string(static_cast<int>(http_context.response->code))},
      {kClientReturnCodeLabel, std::to_string(result.status_code)},
      {kPbsClaimedIdentityLabel,
       GetClaimedIdentityOrUnknownValue(http_context)}};

  opentelemetry::context::Context context;
  client_response_body_size_->Record(http_context.response->body.length, labels,
                                     context);
}

}  // namespace privacy_sandbox::pbs_common
