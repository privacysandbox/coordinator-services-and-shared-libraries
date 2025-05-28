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

#include "cc/core/http2_server/src/http2_server.h"

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nghttp2/asio_http2_server.h>
#include <nlohmann/json.hpp>

#include "cc/core/common/concurrent_map/src/error_codes.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/http2_server/src/error_codes.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/core/interface/errors.h"
#include "cc/core/utils/src/base64.h"
#include "cc/core/utils/src/http.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

namespace privacy_sandbox::pbs_common {

namespace {

using ::boost::asio::ssl::context;
using ::boost::system::error_code;
using ::boost::system::errc::success;
using ::nghttp2::asio_http2::server::configure_tls_context_easy;
using ::nghttp2::asio_http2::server::request;
using ::nghttp2::asio_http2::server::response;
using ::opentelemetry::sdk::resource::SemanticConventions::kHttpRequestMethod;
using ::opentelemetry::sdk::resource::SemanticConventions::
    kHttpResponseStatusCode;
using ::opentelemetry::sdk::resource::SemanticConventions::kHttpRoute;
using ::opentelemetry::sdk::resource::SemanticConventions::kServerAddress;
using ::opentelemetry::sdk::resource::SemanticConventions::kServerPort;

static constexpr char kHttp2Server[] = "Http2Server";
static constexpr size_t kConnectionReadTimeoutInSeconds = 90;

static const std::set<HttpStatusCode> kHttpStatusCode4xxMap = {
    HttpStatusCode::BAD_REQUEST,
    HttpStatusCode::UNAUTHORIZED,
    HttpStatusCode::FORBIDDEN,
    HttpStatusCode::NOT_FOUND,
    HttpStatusCode::METHOD_NOT_ALLOWED,
    HttpStatusCode::REQUEST_TIMEOUT,
    HttpStatusCode::CONFLICT,
    HttpStatusCode::GONE,
    HttpStatusCode::LENGTH_REQUIRED,
    HttpStatusCode::PRECONDITION_FAILED,
    HttpStatusCode::REQUEST_ENTITY_TOO_LARGE,
    HttpStatusCode::REQUEST_URI_TOO_LONG,
    HttpStatusCode::UNSUPPORTED_MEDIA_TYPE,
    HttpStatusCode::REQUEST_RANGE_NOT_SATISFIABLE,
    HttpStatusCode::MISDIRECTED_REQUEST,
    HttpStatusCode::TOO_MANY_REQUESTS};

static const std::set<HttpStatusCode> kHttpStatusCode5xxMap = {
    HttpStatusCode::INTERNAL_SERVER_ERROR,
    HttpStatusCode::NOT_IMPLEMENTED,
    HttpStatusCode::BAD_GATEWAY,
    HttpStatusCode::SERVICE_UNAVAILABLE,
    HttpStatusCode::GATEWAY_TIMEOUT,
    HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED};

// Checks if the x-auth-token contains a field that only an AWS token would
// contain to decide whether to use the AWS authorization proxy. This is to
// authenticate requests that come from AWS PBS to GCP PBS via DNS.
bool UseAwsAuthorizationProxy(
    const AuthorizationMetadata& authorization_metadata) {
  ExecutionResultOr<std::string> padded_token =
      PadBase64Encoding(authorization_metadata.authorization_token);
  if (!padded_token.Successful()) {
    return false;
  }

  std::string token;
  if (ExecutionResult execution_result = Base64Decode(*padded_token, token);
      !execution_result.Successful()) {
    return false;
  }
  nlohmann::json json_token;
  try {
    json_token = nlohmann::json::parse(token);
  } catch (...) {
    return false;
  }
  static constexpr absl::string_view kAmzDate = "amz_date";
  return json_token.contains(kAmzDate);
}

/*
 * @brief Sets up the synchronization context by retrieving it from the active
 * requests map (or creating if it doesn't exist). It assigns the handler,
 * context, and necessary callbacks for managing the HTTP2 request.
 *
 * @param http2_context The async context for the HTTP2 request and response.
 * @param http_handler The HTTP handler function to process requests.
 * @param active_requests A concurrent map containing active requests, mapped by
 * UUID.
 * @param sync_context A shared pointer to the synchronization context for the
 * HTTP2 request.
 *
 * @return ExecutionResult indicating success or failure of setting up the sync
 * context.
 */
ExecutionResult SetSyncContext(
    const AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
    const HttpHandler& http_handler,
    ConcurrentMap<Uuid,
                  std::shared_ptr<Http2Server::Http2SynchronizationContext>,
                  UuidCompare>& active_requests,
    std::shared_ptr<Http2Server::Http2SynchronizationContext>& sync_context) {
  ExecutionResult execution_result;

  execution_result =
      active_requests.Find(http2_context.request->id, sync_context);
  if (!execution_result.Successful()) {
    SCP_DEBUG_CONTEXT(kHttp2Server, http2_context,
                      "[HandleHttp2Request] Cannot find the sync context in "
                      "the active requests map! Creating new sync context and "
                      "adding to the active requests map!");

    sync_context = std::make_shared<Http2Server::Http2SynchronizationContext>();

    auto context_pair = std::make_pair(http2_context.request->id, sync_context);
    execution_result = active_requests.Insert(context_pair, sync_context);
    if (!execution_result.Successful()) {
      SCP_ERROR_CONTEXT(kHttp2Server, http2_context, execution_result,
                        "[HandleHttp2Request] Cannot insert the sync context "
                        "to the active requests map!");
      return execution_result;
    }
  }

  sync_context->pending_callbacks = 2;  // 1 for authorization, 1 for body data.
  sync_context->http2_context = http2_context;
  sync_context->http_handler = http_handler;
  sync_context->failed = false;

  return SuccessExecutionResult();
}
}  // namespace

Http2Server::~Http2Server() {
  if (active_requests_instrument_) {
    active_requests_instrument_->RemoveCallback(
        reinterpret_cast<opentelemetry::metrics::ObservableCallbackPtr>(
            &Http2Server::ObserveActiveRequestsCallback),
        this);
  }
}

ExecutionResult Http2Server::OtelMetricInit() noexcept {
  if (!metric_router_) {
    return SuccessExecutionResult();
  }

  meter_ = metric_router_->GetOrCreateMeter(kHttp2ServerMeter);

  metric_router_->CreateViewForInstrument(
      /*meter_name=*/kHttp2ServerMeter,
      /*instrument_name=*/kServerRequestDurationMetric,
      /*instrument_type=*/
      opentelemetry::sdk::metrics::InstrumentType::kHistogram,
      /*aggregation_type=*/
      opentelemetry::sdk::metrics::AggregationType::kHistogram,
      /*boundaries=*/MakeLatencyHistogramBoundaries(),
      /*version=*/"", /*schema=*/"",
      /*view_description=*/"Server Request Duration Histogram",
      /*unit=*/kSecondUnit);

  server_request_duration_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<double>>(
          metric_router_->GetOrCreateSyncInstrument(
              kServerRequestDurationMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateDoubleHistogram(
                    kServerRequestDurationMetric,
                    "Duration of HTTP server requests.", kSecondUnit);
              }));

  active_requests_instrument_ = metric_router_->GetOrCreateObservableInstrument(
      kActiveRequestsMetric,
      [&]() -> std::shared_ptr<opentelemetry::metrics::ObservableInstrument> {
        return meter_->CreateInt64ObservableGauge(
            kActiveRequestsMetric, "Number of active HTTP server requests.");
      });

  active_requests_instrument_->AddCallback(
      reinterpret_cast<opentelemetry::metrics::ObservableCallbackPtr>(
          &Http2Server::ObserveActiveRequestsCallback),
      this);

  server_request_body_size_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<uint64_t>>(
          metric_router_->GetOrCreateSyncInstrument(
              kServerRequestBodySizeMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateUInt64Histogram(
                    kServerRequestBodySizeMetric,
                    "Server request body size in Bytes - uncompressed.",
                    kByteUnit);
              }));

  server_response_body_size_ =
      std::static_pointer_cast<opentelemetry::metrics::Histogram<uint64_t>>(
          metric_router_->GetOrCreateSyncInstrument(
              kServerResponseBodySizeMetric,
              [&]() -> std::shared_ptr<
                        opentelemetry::metrics::SynchronousInstrument> {
                return meter_->CreateUInt64Histogram(
                    kServerResponseBodySizeMetric,
                    "Server response body size in Bytes - uncompressed.",
                    kByteUnit);
              }));

  return SuccessExecutionResult();
}

ExecutionResult Http2Server::Init() noexcept {
  if (use_tls_) {
    error_code nghttp2_error_code;

    try {
      tls_context_.use_private_key_file(private_key_file_, context::pem);
      tls_context_.use_certificate_chain_file(certificate_chain_file_);
      configure_tls_context_easy(nghttp2_error_code, tls_context_);
    } catch (...) {
      auto execution_result = FailureExecutionResult(
          SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT);
      SCP_ERROR(kHttp2Server, kZeroUuid, execution_result,
                "Failed to initialize TLS context.");
      return execution_result;
    }

    if (nghttp2_error_code.value() != success) {
      auto execution_result = FailureExecutionResult(
          SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT);
      SCP_ERROR(kHttp2Server, kZeroUuid, execution_result,
                absl::StrFormat(
                    "Failed to initialize TLS context with error code: %d",
                    nghttp2_error_code.value()));
      return execution_result;
    }
  }

  // Otel metrics setup.
  RETURN_IF_FAILURE(OtelMetricInit());

  return SuccessExecutionResult();
}

ExecutionResult Http2Server::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(SC_HTTP2_SERVER_ALREADY_RUNNING);
  }

  is_running_ = true;

  std::vector<std::string> paths;
  auto execution_result = resource_handlers_.Keys(paths);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  for (const auto& path : paths) {
    // TODO: here we are binding a universal handler, and the real
    // handler is looked up again inside it. Ideally, we can do the look up
    // here, and pass the result to std::bind(), to save runtime cost.
    http2_server_.handle(
        path, std::bind(&Http2Server::OnHttp2Request, this,
                        std::placeholders::_1, std::placeholders::_2));
  }

  http2_server_.read_timeout(
      boost::posix_time::seconds(kConnectionReadTimeoutInSeconds));
  http2_server_.num_threads(thread_pool_size_);

  error_code nghttp2_error_code;
  error_code server_listen_and_serve_error_code;
  const bool asynchronous = true;

  if (use_tls_) {
    server_listen_and_serve_error_code = http2_server_.listen_and_serve(
        nghttp2_error_code, tls_context_, host_address_, port_, asynchronous);
  } else {
    server_listen_and_serve_error_code = http2_server_.listen_and_serve(
        nghttp2_error_code, host_address_, port_, asynchronous);
  }

  if (server_listen_and_serve_error_code) {
    return FailureExecutionResult(SC_HTTP2_SERVER_INITIALIZATION_FAILED);
  }

  return SuccessExecutionResult();
}

ExecutionResult Http2Server::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(SC_HTTP2_SERVER_ALREADY_STOPPED);
  }

  is_running_ = false;
  try {
    http2_server_.stop();
    for (auto& io_service : http2_server_.io_services()) {
      io_service->stop();
    }
    http2_server_.join();
  } catch (...) {
    // Doing the best to stop, ignore otherwise.
  }

  return SuccessExecutionResult();
}

ExecutionResult Http2Server::RegisterResourceHandler(
    HttpMethod http_method, std::string& path, HttpHandler& handler) noexcept {
  if (is_running_) {
    return FailureExecutionResult(SC_HTTP2_SERVER_CANNOT_REGISTER_HANDLER);
  }
  auto verb_to_handler_map =
      std::make_shared<ConcurrentMap<HttpMethod, HttpHandler>>();
  auto path_to_map_pair = std::make_pair(path, verb_to_handler_map);

  auto execution_result =
      resource_handlers_.Insert(path_to_map_pair, verb_to_handler_map);
  if (!execution_result.Successful()) {
    if (execution_result !=
        FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)) {
      return execution_result;
    }
  }

  auto verb_to_handler_pair = std::make_pair(http_method, handler);
  return verb_to_handler_map->Insert(verb_to_handler_pair, handler);
}

void Http2Server::OnHttp2Request(const request& request,
                                 const response& response) noexcept {
  // Measure the entry time to track request-response latency
  std::chrono::time_point<std::chrono::steady_clock> entry_time =
      std::chrono::steady_clock::now();
  auto parent_activity_id = Uuid::GenerateUuid();
  auto http2Request = std::make_shared<NgHttp2Request>(request);
  auto request_endpoint_type = RequestTargetEndpointType::Local;

  // This is the entry point of a Http2Request.
  // The Http2Request ID that we generate here is used as the correlation ID
  // throughout the lifetime of this context and subsequent child contexts.
  AsyncContext<NgHttp2Request, NgHttp2Response> http2_context(
      http2Request,
      std::bind(&Http2Server::OnHttp2Response, this, std::placeholders::_1,
                request_endpoint_type),
      parent_activity_id, http2Request->id);

  http2_context.response = std::make_shared<NgHttp2Response>(response);
  http2_context.response->headers = std::make_shared<HttpHeaders>();

  auto sync_context = std::make_shared<Http2SynchronizationContext>();
  sync_context->entry_time = entry_time;
  auto context_pair = std::make_pair(http2Request->id, sync_context);
  auto execution_result = active_requests_.Insert(context_pair, sync_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kHttp2Server, http2_context, execution_result,
                      "[OnHttp2Request] Cannot insert the sync context to "
                      "the active requests map!");
    FinishContext(execution_result, http2_context);
    return;
  }

  SCP_DEBUG_CONTEXT(kHttp2Server, http2_context, "Received a http2 request");

  execution_result = http2_context.request->UnwrapNgHttp2Request();
  if (!execution_result.Successful()) {
    http2_context.result = execution_result;
    http2_context.Finish();
    return;
  }

  // Check if path is registered
  std::shared_ptr<ConcurrentMap<HttpMethod, HttpHandler>> resource_handler;
  execution_result = resource_handlers_.Find(
      http2_context.request->handler_path, resource_handler);
  if (!execution_result.Successful()) {
    http2_context.result = execution_result;
    http2_context.Finish();
    return;
  }

  // Check if there is an active handler for the specific method.
  HttpHandler http_handler;
  execution_result =
      resource_handler->Find(http2_context.request->method, http_handler);
  if (!execution_result.Successful()) {
    http2_context.result = execution_result;
    http2_context.Finish();
    return;
  }

  return HandleHttp2Request(http2_context, http_handler);
}

void Http2Server::HandleHttp2Request(
    AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
    HttpHandler& http_handler) noexcept {
  // We should not wait for the whole request body to be received since this
  // can be a source for attacks. What is done here is to validate the
  // authorization token in parallel. If the authorization fails, the response
  // will be sent immediately, if it is successful the flow will proceed.

  std::shared_ptr<Http2SynchronizationContext> sync_context;
  auto execution_result = SetSyncContext(http2_context, http_handler,
                                         active_requests_, sync_context);
  if (!execution_result.Successful()) {
    FinishContext(execution_result, http2_context);
    return;
  }

  auto authorization_request = std::make_shared<AuthorizationProxyRequest>();
  auto& headers = http2_context.request->headers;

  if (headers) {
    auto auth_headers_iter = headers->find(kAuthHeader);
    if (auth_headers_iter != headers->end()) {
      authorization_request->authorization_metadata.authorization_token =
          auth_headers_iter->second;
    }

    auth_headers_iter = headers->find(kClaimedIdentityHeader);
    if (auth_headers_iter != headers->end()) {
      authorization_request->authorization_metadata.claimed_identity =
          auth_headers_iter->second;
    }
  }

  SCP_DEBUG_CONTEXT(
      kHttp2Server, http2_context,
      absl::StrFormat(
          "Sending authorization request for request with path: %s, "
          "claimed identity: %s",
          http2_context.request->handler_path.c_str(),
          authorization_request->authorization_metadata.claimed_identity
              .c_str()));

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_context(authorization_request,
                            std::bind(&Http2Server::OnAuthorizationCallback,
                                      this, std::placeholders::_1,
                                      http2_context.request->id, sync_context),
                            http2_context);

  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy_to_use =
      authorization_proxy_;

  bool dns_routing_enabled = false;
  if (config_provider_ != nullptr &&
      config_provider_->Get(kHttpServerDnsRoutingEnabled, dns_routing_enabled)
          .Successful() &&
      dns_routing_enabled) {
    if (aws_authorization_proxy_ != nullptr &&
        UseAwsAuthorizationProxy(
            authorization_context.request->authorization_metadata)) {
      authorization_proxy_to_use = aws_authorization_proxy_;
      SCP_DEBUG_CONTEXT(kHttp2Server, http2_context,
                        "Switching to AWS Authorization Proxy.");
    }
  }

  operation_dispatcher_.Dispatch<
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>>(
      authorization_context,
      [authorization_proxy = authorization_proxy_to_use](
          AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
              authorization_context) {
        return authorization_proxy->Authorize(authorization_context);
      });

  // Set the callbacks for receiving data on the request and cleaning up
  // request. The callbacks will start getting invoked as soon as we return
  // this thread back to nghttp2 i.e. below. To ensure our error processing
  // does not conflict with the nghttp2 callback invocations, the callbacks
  // are set right before we give back the thread to nghttp2.
  //
  // NOTE: these callbacks are not invoked concurrently. The NgHttp2Server
  // does an event loop on a given thread for all events that happen on a
  // request, so any subsequent callbacks of the request for recieving data or
  // close will not be processed until this function exits.
  //
  // Request's event loop (all happen sequentially on same thread) is as
  // following
  // 1. Connection Established (this method gets invoked)
  // 2. Data is recieved (request.on_request_body_received is
  // invoked)
  // 3. Connection is terminated (response.on_closed is invoked)
  //
  http2_context.request->SetOnRequestBodyDataReceivedCallback(
      std::bind(&Http2Server::OnHttp2PendingCallback, this,
                std::placeholders::_1, http2_context.request->id));
  http2_context.response->SetOnCloseCallback(
      std::bind(&Http2Server::OnHttp2Cleanup, this, std::cref(*sync_context),
                std::placeholders::_1));
}

void Http2Server::OnAuthorizationCallback(
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
        authorization_context,
    Uuid& request_id,
    const std::shared_ptr<Http2SynchronizationContext>& sync_context) noexcept {
  if (!authorization_context.result.Successful()) {
    SCP_DEBUG_CONTEXT(kHttp2Server, authorization_context,
                      "Authorization failed.");
  } else {
    sync_context->http2_context.request->auth_context
        .authorized_domain = std::make_shared<std::string>(
        authorization_context.request->authorization_metadata.claimed_identity);
    sync_context->http2_context.request->auth_context.authorized_domain =
        authorization_context.response->authorized_metadata.authorized_domain;
  }

  OnHttp2PendingCallback(authorization_context.result, request_id);
}

void Http2Server::OnHttp2PendingCallback(
    ExecutionResult callback_execution_result,
    const Uuid& request_id) noexcept {
  // Lookup the sync context
  std::shared_ptr<Http2SynchronizationContext> sync_context;
  auto execution_result = active_requests_.Find(request_id, sync_context);
  if (!execution_result.Successful()) {
    SCP_DEBUG(kHttp2Server, request_id,
              "Could not find Http2SynchronizationContext(current request) in "
              "active requests map. This could happen if the request was "
              "already finished or if the request ID is invalid.");
    return;
  }

  if (!callback_execution_result.Successful()) {
    auto failed = false;
    // Only change if the current status was false.
    if (sync_context->failed.compare_exchange_strong(failed, true)) {
      sync_context->http2_context.result = callback_execution_result;
      sync_context->http2_context.Finish();
    }
  }

  if (sync_context->pending_callbacks.fetch_sub(1) != 1) {
    return;
  }

  if (sync_context->failed.load()) {
    // If it is failed, the callback has been called before.
    return;
  }

  AsyncContext<HttpRequest, HttpResponse> http_context;
  // Reuse the same activity IDs for correlation down the line.
  http_context.parent_activity_id =
      sync_context->http2_context.parent_activity_id;
  http_context.activity_id = sync_context->http2_context.activity_id;
  http_context.correlation_id = sync_context->http2_context.correlation_id;
  http_context.request = std::static_pointer_cast<HttpRequest>(
      sync_context->http2_context.request);
  http_context.response = std::static_pointer_cast<HttpResponse>(
      sync_context->http2_context.response);
  http_context.callback =
      [this, http2_context = sync_context->http2_context](
          AsyncContext<HttpRequest, HttpResponse>& http_context) mutable {
        http2_context.result = http_context.result;
        // At this point the request is being handled locally.
        OnHttp2Response(http2_context, RequestTargetEndpointType::Local);
      };

  // Recording request body length in Bytes - request body is received when code
  // reaches here.
  RecordRequestBodySize(sync_context->http2_context);

  execution_result = sync_context->http_handler(http_context);
  if (!execution_result.Successful()) {
    sync_context->http2_context.result = execution_result;
    sync_context->http2_context.Finish();
    return;
  }
}

void Http2Server::OnHttp2Response(
    AsyncContext<NgHttp2Request, NgHttp2Response>& http_context,
    RequestTargetEndpointType endpoint_type) noexcept {
  http_context.response->code = HttpStatusCode::OK;
  if (!http_context.result.Successful()) {
    auto error_code = GetErrorHttpStatusCode(http_context.result.status_code);
    http_context.response->code = error_code;
    SCP_ERROR_CONTEXT(
        kHttp2Server, http_context, http_context.result,
        absl::StrFormat(
            "http2 request finished with error. http status code: '%d', "
            "request endpoint type: '%d'",
            static_cast<typename std::underlying_type<HttpStatusCode>::type>(
                http_context.response->code),
            static_cast<size_t>(endpoint_type)));
  } else {
    SCP_DEBUG_CONTEXT(
        kHttp2Server, http_context,
        absl::StrFormat("http2 request finished. http status code: 200, "
                        "request endpoint type: '%d'",
                        static_cast<size_t>(endpoint_type)));
  }

  // Record response body size in Bytes - response is prepared here to be sent.
  RecordResponseBodySize(http_context);

  // Capture the shared_ptr to keep the response object alive when the work
  // actually starts executing. Do not execute response->Send() on a thread
  // that does not belong to nghttp2response as it could lead to concurrency
  // issues so always post the work to send response to the IoService.
  http_context.response->SubmitWorkOnIoService(
      [response = http_context.response]() { response->Send(); });
}

void Http2Server::OnHttp2Cleanup(
    const Http2SynchronizationContext& sync_context,
    uint32_t error_code) noexcept {
  auto request_id_str = ToString(sync_context.http2_context.request->id);
  if (error_code != 0) {
    SCP_DEBUG(
        kHttp2Server, sync_context.http2_context.parent_activity_id,
        absl::StrFormat(
            "The connection for request ID %s was closed with status code %d",
            request_id_str, error_code));
  }
  RecordServerLatency(sync_context);
  // sync_context should not be used after this line because it has been
  // deallocated.
  active_requests_.Erase(sync_context.http2_context.request->id);
}

absl::flat_hash_map<absl::string_view, std::string>
Http2Server::GetOtelMetricLabels(
    const AsyncContext<NgHttp2Request, NgHttp2Response>& http_context) {
  absl::flat_hash_map<std::string_view, std::string> labels = {
      {kServerAddress, host_address_},
      {kServerPort, port_},
      {kHttpRoute, http_context.request->handler_path},
      {kHttpRequestMethod, HttpMethodToString(http_context.request->method)},
      {kPbsClaimedIdentityLabel,
       GetClaimedIdentityOrUnknownValue(http_context)},
      {kScpHttpRequestClientVersionLabel,
       GetUserAgentOrUnknownValue(http_context)}};

  if (http_context.response != nullptr) {
    labels.try_emplace(
        kHttpResponseStatusCode,
        std::to_string(static_cast<int>(http_context.response->code)));
  }

  if (std::string* auth_domain =
          http_context.request->auth_context.authorized_domain.get();
      auth_domain != nullptr) {
    labels.try_emplace(kPbsAuthDomainLabel, *auth_domain);
  }

  return labels;
}

void Http2Server::RecordServerLatency(
    const Http2SynchronizationContext& sync_context) {
  if (!server_request_duration_) {
    return;
  }

  std::chrono::duration<double> latency =
      std::chrono::steady_clock::now() - sync_context.entry_time;
  double latency_s = latency / std::chrono::seconds(1);

  absl::flat_hash_map<absl::string_view, std::string> labels =
      GetOtelMetricLabels(sync_context.http2_context);

  opentelemetry::context::Context context;
  server_request_duration_->Record(latency_s, labels, context);
}

void Http2Server::RecordRequestBodySize(
    const AsyncContext<NgHttp2Request, NgHttp2Response>& http_context) {
  if (!server_request_body_size_) {
    return;
  }

  absl::flat_hash_map<absl::string_view, std::string> labels =
      GetOtelMetricLabels(http_context);

  opentelemetry::context::Context context;
  server_request_body_size_->Record(http_context.request->body.length, labels,
                                    context);
}

void Http2Server::RecordResponseBodySize(
    const AsyncContext<NgHttp2Request, NgHttp2Response>& http_context) {
  if (!server_response_body_size_) {
    return;
  }

  absl::flat_hash_map<absl::string_view, std::string> labels =
      GetOtelMetricLabels(http_context);

  opentelemetry::context::Context context;
  server_response_body_size_->Record(http_context.response->body.length, labels,
                                     context);
}

void Http2Server::ObserveActiveRequestsCallback(
    opentelemetry::metrics::ObserverResult observer_result,
    absl::Nonnull<Http2Server*> self_ptr) {
  auto observer = std::get<
      std::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
      observer_result);
  observer->Observe(self_ptr->active_requests_.Size());
}

}  // namespace privacy_sandbox::pbs_common
