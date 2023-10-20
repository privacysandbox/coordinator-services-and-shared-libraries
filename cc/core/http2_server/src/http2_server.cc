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

#include "http2_server.h"

#include <nghttp2/asio_http2_server.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/authorization_proxy/src/error_codes.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/errors.h"
#include "core/interface/metrics_def.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "http2_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"
#include "public/cpio/utils/metric_aggregation/src/aggregate_metric.h"

using absl::StrCat;
using boost::asio::ssl::context;
using boost::system::error_code;
using boost::system::errc::success;
using google::scp::core::common::ConcurrentMap;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::errors::GetErrorHttpStatusCode;
using google::scp::core::errors::HttpStatusCode;
using google::scp::cpio::AggregateMetric;
using google::scp::cpio::AggregateMetricInterface;
using google::scp::cpio::kCountSecond;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricLabelsBase;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using nghttp2::asio_http2::server::configure_tls_context_easy;
using nghttp2::asio_http2::server::request;
using nghttp2::asio_http2::server::response;
using std::bind;
using std::make_pair;
using std::make_shared;
using std::move;
using std::set;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::thread;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;

static constexpr char kHttp2Server[] = "Http2Server";

static constexpr size_t kConnectionReadTimeoutInSeconds = 90;

static const set<HttpStatusCode> kHttpStatusCode4xxMap = {
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

static const set<HttpStatusCode> kHttpStatusCode5xxMap = {
    HttpStatusCode::INTERNAL_SERVER_ERROR,
    HttpStatusCode::NOT_IMPLEMENTED,
    HttpStatusCode::BAD_GATEWAY,
    HttpStatusCode::SERVICE_UNAVAILABLE,
    HttpStatusCode::GATEWAY_TIMEOUT,
    HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED};

namespace google::scp::core {
ExecutionResult Http2Server::MetricInit() noexcept {
  auto metric_name = make_shared<MetricName>(kMetricNameHttpRequest);
  auto metric_unit = make_shared<MetricUnit>(kCountSecond);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(kHttp2Server);
  metric_info->labels =
      make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  vector<string> event_list = {kMetricEventHttpUnableToResolveRoute,
                               kMetricEventHttp2xxLocal,
                               kMetricEventHttp4xxLocal,
                               kMetricEventHttp5xxLocal,
                               kMetricEventHttp2xxForwarded,
                               kMetricEventHttp4xxForwarded,
                               kMetricEventHttp5xxForwarded};
  http_request_metrics_ = make_shared<AggregateMetric>(
      async_executor_, metric_client_, metric_info,
      aggregated_metric_interval_ms_, make_shared<vector<string>>(event_list));
  return http_request_metrics_->Init();
}

ExecutionResult Http2Server::MetricRun() noexcept {
  return http_request_metrics_->Run();
}

ExecutionResult Http2Server::MetricStop() noexcept {
  return http_request_metrics_->Stop();
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
          core::errors::SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT);
      SCP_ERROR(kHttp2Server, kZeroUuid, execution_result,
                "Failed to initialize TLS context.");
      return execution_result;
    }

    if (nghttp2_error_code.value() != success) {
      auto execution_result = FailureExecutionResult(
          core::errors::SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT);
      SCP_ERROR(kHttp2Server, kZeroUuid, execution_result,
                "Failed to initialize TLS context with error code: %d",
                nghttp2_error_code.value());
      return execution_result;
    }
  }

  if (metric_client_) {
    if (!config_provider_ ||
        !config_provider_
             ->Get(kAggregatedMetricIntervalMs, aggregated_metric_interval_ms_)
             .Successful()) {
      aggregated_metric_interval_ms_ = kDefaultAggregatedMetricIntervalMs;
    }

    RETURN_IF_FAILURE(MetricInit());
  }

  bool request_routing_enabled = false;
  if (config_provider_ &&
      config_provider_
          ->Get(kHTTPServerRequestRoutingEnabled, request_routing_enabled)
          .Successful()) {
    SCP_INFO(kHttp2Server, kZeroUuid, "Request routing is enabled");
    request_routing_enabled_ = request_routing_enabled;
  }

  bool site_based_auth_enabled = false;
  if (config_provider_ &&
      config_provider_
          ->Get(kPBSAuthorizationEnableSiteBasedAuthorization,
                site_based_auth_enabled)
          .Successful()) {
    site_based_auth_enabled_ = site_based_auth_enabled;
  }

  return SuccessExecutionResult();
}

ExecutionResult Http2Server::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(errors::SC_HTTP2_SERVER_ALREADY_RUNNING);
  }

  is_running_ = true;

  if (metric_client_) {
    auto execution_result = MetricRun();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  vector<string> paths;
  auto execution_result = resource_handlers_.Keys(paths);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  for (const auto& path : paths) {
    // TODO: here we are binding a universal handler, and the real
    // handler is looked up again inside it. Ideally, we can do the look up
    // here, and pass the result to bind(), to save runtime cost.
    http2_server_.handle(path,
                         bind(&Http2Server::OnHttp2Request, this, _1, _2));
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
    return FailureExecutionResult(
        core::errors::SC_HTTP2_SERVER_INITIALIZATION_FAILED);
  }

  return SuccessExecutionResult();
}

ExecutionResult Http2Server::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_HTTP2_SERVER_ALREADY_STOPPED);
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

  if (metric_client_) {
    return MetricStop();
  }
  return SuccessExecutionResult();
}

bool Http2Server::IsRequestForwardingEnabled() const {
  return request_routing_enabled_ && request_route_resolver_ && request_router_;
}

ExecutionResult Http2Server::RegisterResourceHandler(
    HttpMethod http_method, std::string& path, HttpHandler& handler) noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        errors::SC_HTTP2_SERVER_CANNOT_REGISTER_HANDLER);
  }
  auto verb_to_handler_map =
      make_shared<ConcurrentMap<HttpMethod, HttpHandler>>();
  auto path_to_map_pair = make_pair(path, verb_to_handler_map);

  auto execution_result =
      resource_handlers_.Insert(path_to_map_pair, verb_to_handler_map);
  if (!execution_result.Successful()) {
    if (execution_result !=
        FailureExecutionResult(
            errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)) {
      return execution_result;
    }
  }

  auto verb_to_handler_pair = make_pair(http_method, handler);
  return verb_to_handler_map->Insert(verb_to_handler_pair, handler);
}

void Http2Server::OnHttp2Request(const request& request,
                                 const response& response) noexcept {
  auto parent_activity_id = Uuid::GenerateUuid();
  auto http2Request = make_shared<NgHttp2Request>(request);
  auto request_endpoint_type = RequestTargetEndpointType::Unknown;
  if (!IsRequestForwardingEnabled()) {
    request_endpoint_type = RequestTargetEndpointType::Local;
  }

  // This is the entry point of a Http2Request.
  // The Http2Request ID that we generate here is used as the correlation ID
  // throughout the lifetime of this context and subsequent child contexts.
  AsyncContext<NgHttp2Request, NgHttp2Response> http2_context(
      http2Request,
      bind(&Http2Server::OnHttp2Response, this, _1, request_endpoint_type),
      parent_activity_id, http2Request->id);
  http2_context.response = make_shared<NgHttp2Response>(response);
  http2_context.response->headers = make_shared<core::HttpHeaders>();

  SCP_DEBUG_CONTEXT(kHttp2Server, http2_context, "Received a http2 request");

  auto execution_result = http2_context.request->UnwrapNgHttp2Request();
  if (!execution_result.Successful()) {
    http2_context.result = execution_result;
    http2_context.Finish();
    return;
  }

  // Check if path is registered
  shared_ptr<ConcurrentMap<HttpMethod, HttpHandler>> resource_handler;
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

  return RouteOrHandleHttp2Request(http2_context, http_handler);
}

void Http2Server::RouteOrHandleHttp2Request(
    AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
    HttpHandler& http_handler) noexcept {
  if (IsRequestForwardingEnabled()) {
    auto endpoint_info =
        request_route_resolver_->ResolveRoute(*http2_context.request);
    if (!endpoint_info.has_value()) {
      SCP_ERROR_CONTEXT(kHttp2Server, http2_context, endpoint_info.result(),
                        "Cannot resolve request endpoint");
      // Set a retriable error and send it back to the client.
      auto execution_result = FailureExecutionResult(
          core::errors::SC_HTTP2_SERVER_FAILED_TO_RESOLVE_ROUTE);
      FinishContext(execution_result, http2_context);
      return;
    }

    SCP_DEBUG_CONTEXT(kHttp2Server, http2_context,
                      "Resolved route to endpoint '%s', IsLocalEndpoint: '%d'",
                      endpoint_info->uri->c_str(),
                      endpoint_info->is_local_endpoint);

    if (!endpoint_info->is_local_endpoint) {
      // Rebind the callback with the updated request target type
      http2_context.callback = bind(&Http2Server::OnHttp2Response, this, _1,
                                    RequestTargetEndpointType::Remote);
      // Perform routing when request data is obtained on the connection. If the
      // connection is closed, do OnHttp2CleanupRoutedRequest.
      http2_context.request->SetOnRequestBodyDataReceivedCallback(
          bind(&Http2Server::OnHttp2RequestDataObtainedRoutedRequest, this,
               http2_context, *endpoint_info, _1));
      http2_context.response->SetOnCloseCallback(
          bind(&Http2Server::OnHttp2CleanupOfRoutedRequest, this,
               http2_context.request->id, http2_context.request->id, _1));
      return;
    }
    // Rebind the callback with the updated request target type
    http2_context.callback = bind(&Http2Server::OnHttp2Response, this, _1,
                                  RequestTargetEndpointType::Local);
    // Local endpoint handling continues below.
  }

  return HandleHttp2Request(http2_context, http_handler);
}

void Http2Server::OnHttp2RequestDataObtainedRoutedRequest(
    AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
    const RequestRouteEndpointInfo& endpoint_info,
    ExecutionResult request_body_received_result) noexcept {
  if (!request_body_received_result.Successful()) {
    // If request data is not obtained fully, the request cannot be routed.
    FinishContext(request_body_received_result, http2_context);
    return;
  }

  // Typecast to avoid copying data when constructing a new context.
  AsyncContext<HttpRequest, HttpResponse> routing_context(
      static_pointer_cast<HttpRequest>(http2_context.request),
      std::bind(&Http2Server::OnRoutingResponseReceived, this, http2_context,
                _1),
      http2_context);
  // The target path should reflect the forwarding endpoint.
  routing_context.request->path = make_shared<string>(
      StrCat(*endpoint_info.uri, http2_context.request->handler_path));

  auto execution_result = request_router_->RouteRequest(routing_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kHttp2Server, http2_context, execution_result,
                      "Cannot route request");
    // Set a retriable error and send it back to the client.
    execution_result =
        FailureExecutionResult(core::errors::SC_HTTP2_SERVER_FAILED_TO_ROUTE);
    FinishContext(execution_result, http2_context);
  }
}

void Http2Server::OnRoutingResponseReceived(
    AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
    AsyncContext<HttpRequest, HttpResponse>& routing_context) noexcept {
  if (!routing_context.result.Successful()) {
    FinishContext(routing_context.result, http2_context);
    return;
  }
  http2_context.result = routing_context.result;
  http2_context.response->body = routing_context.response->body;
  http2_context.response->headers = routing_context.response->headers;
  http2_context.response->code = routing_context.response->code;
  FinishContext(http2_context.result, http2_context);
}

void Http2Server::HandleHttp2Request(
    AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
    HttpHandler& http_handler) noexcept {
  // We should not wait for the whole request body to be received since this
  // can be a source for attacks. What is done here is to validate the
  // authorization token in parallel. If the authorization fails, the response
  // will be sent immediately, if it is successful the flow will proceed.

  auto sync_context = make_shared<Http2SynchronizationContext>();
  sync_context->pending_callbacks = 2;  // 1 for authorization, 1 for body data.
  sync_context->http2_context = http2_context;
  sync_context->http_handler = http_handler;
  sync_context->failed = false;

  auto context_pair = make_pair(http2_context.request->id, sync_context);
  auto execution_result = active_requests_.Insert(context_pair, sync_context);
  if (!execution_result.Successful()) {
    http2_context.result = execution_result;
    http2_context.Finish();
    return;
  }

  auto authorization_request = make_shared<AuthorizationProxyRequest>();
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
      "Sending authorization request for request with path: %s, "
      "claimed identity: %s",
      http2_context.request->handler_path.c_str(),
      authorization_request->authorization_metadata.claimed_identity.c_str());

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_context(authorization_request,
                            bind(&Http2Server::OnAuthorizationCallback, this,
                                 _1, http2_context.request->id, sync_context),
                            http2_context);

  operation_dispatcher_.Dispatch<
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>>(
      authorization_context,
      [authorization_proxy = authorization_proxy_](
          AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
              authorization_context) {
        return authorization_proxy->Authorize(authorization_context);
      });

  // Set the callbacks for receiving data on the request and cleaning up
  // request. The callbacks will start getting invoked as soon as we return this
  // thread back to nghttp2 i.e. below. To ensure our error processing does not
  // conflict with the nghttp2 callback invocations, the callbacks are set right
  // before we give back the thread to nghttp2.
  //
  // NOTE: these callbacks are not invoked concurrently. The NgHttp2Server does
  // an event loop on a given thread for all events that happen on a request, so
  // any subsequent callbacks of the request for recieving data or close will
  // not be processed until this function exits.
  //
  // Request's event loop (all happen sequentially on same thread) is as
  // following
  // 1. Connection Established (this method gets invoked)
  // 2. Data is recieved (request.on_request_body_received is
  // invoked)
  // 3. Connection is terminated (response.on_closed is invoked)
  //
  http2_context.request->SetOnRequestBodyDataReceivedCallback(
      bind(&Http2Server::OnHttp2PendingCallback, this, _1,
           http2_context.request->id));
  http2_context.response->SetOnCloseCallback(
      bind(&Http2Server::OnHttp2Cleanup, this, http2_context.request->id,
           http2_context.request->id, _1));
}

void Http2Server::OnAuthorizationCallback(
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
        authorization_context,
    Uuid& request_id,
    const shared_ptr<Http2SynchronizationContext>& sync_context) noexcept {
  if (!authorization_context.result.Successful()) {
    SCP_DEBUG_CONTEXT(kHttp2Server, authorization_context,
                      "Authorization failed.");
  } else {
    if (site_based_auth_enabled_) {
      sync_context->http2_context.request->auth_context.authorized_domain =
          make_shared<string>(authorization_context.request
                                  ->authorization_metadata.claimed_identity);
    } else {
      sync_context->http2_context.request->auth_context.authorized_domain =
          authorization_context.response->authorized_metadata.authorized_domain;
    }
  }

  OnHttp2PendingCallback(authorization_context.result, request_id);
}

void Http2Server::OnHttp2PendingCallback(
    ExecutionResult callback_execution_result,
    const Uuid& request_id) noexcept {
  // Lookup the sync context
  shared_ptr<Http2SynchronizationContext> sync_context;
  auto execution_result = active_requests_.Find(request_id, sync_context);
  if (!execution_result.Successful()) {
    // TODO: Log this.
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
  // Resuse the same activity IDs for correlation down the line.
  http_context.parent_activity_id =
      sync_context->http2_context.parent_activity_id;
  http_context.activity_id = sync_context->http2_context.activity_id;
  http_context.correlation_id = sync_context->http2_context.correlation_id;
  http_context.request =
      static_pointer_cast<HttpRequest>(sync_context->http2_context.request);
  http_context.response =
      static_pointer_cast<HttpResponse>(sync_context->http2_context.response);
  http_context.callback =
      [this, http2_context = sync_context->http2_context](
          AsyncContext<HttpRequest, HttpResponse>& http_context) mutable {
        http2_context.result = http_context.result;
        // At this point the request is being handled locally.
        OnHttp2Response(http2_context, RequestTargetEndpointType::Local);
      };

  execution_result = sync_context->http_handler(http_context);
  if (!execution_result.Successful()) {
    sync_context->http2_context.result = execution_result;
    sync_context->http2_context.Finish();
    return;
  }
}

/**
 * @brief Puts a point into the metric for the HTTP request's error code.
 *
 * @param metric
 * @param error_code
 * @param endpoint_type
 */
static void IncrementHttpResponseMetric(
    shared_ptr<AggregateMetricInterface> metric,
    errors::HttpStatusCode error_code,
    Http2Server::RequestTargetEndpointType endpoint_type) {
  // Unknown state happens when the routing is enabled and the request route
  // cannot be determined. For this, we always send a 5xx error code. See
  if (endpoint_type == Http2Server::RequestTargetEndpointType::Unknown) {
    metric->Increment(kMetricEventHttpUnableToResolveRoute);
    return;
  }

  size_t error_code_value = static_cast<size_t>(error_code);
  bool is_remote =
      (endpoint_type == Http2Server::RequestTargetEndpointType::Remote);
  if (error_code_value >= 200 && error_code_value <= 299) {
    const auto& metric_label =
        is_remote ? kMetricEventHttp2xxForwarded : kMetricEventHttp2xxLocal;
    metric->Increment(metric_label);
  } else if (error_code_value >= 400 && error_code_value <= 499) {
    auto code_pair = kHttpStatusCode4xxMap.find(error_code);
    if (code_pair != kHttpStatusCode4xxMap.end()) {
      const auto& metric_label =
          is_remote ? kMetricEventHttp4xxForwarded : kMetricEventHttp4xxLocal;
      metric->Increment(metric_label);
    }
  } else if (error_code_value >= 500 && error_code_value <= 599) {
    auto code_pair = kHttpStatusCode5xxMap.find(error_code);
    if (code_pair != kHttpStatusCode5xxMap.end()) {
      const auto& metric_label =
          is_remote ? kMetricEventHttp5xxForwarded : kMetricEventHttp5xxLocal;
      metric->Increment(metric_label);
    }
  } else {
    // Ignore rest of the errors for now
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
        "http2 request finished with error. http status code: '%d', "
        "request endpoint type: '%d'",
        static_cast<typename std::underlying_type<HttpStatusCode>::type>(
            http_context.response->code),
        static_cast<size_t>(endpoint_type));
  } else {
    SCP_DEBUG_CONTEXT(kHttp2Server, http_context,
                      "http2 request finished. http status code: 200, "
                      "request endpoint type: '%d'",
                      static_cast<size_t>(endpoint_type));
  }

  // Put metric if available
  if (http_request_metrics_) {
    IncrementHttpResponseMetric(http_request_metrics_,
                                http_context.response->code, endpoint_type);
  }

  // Capture the shared_ptr to keep the response object alive when the work
  // actually starts executing. Do not execute response->Send() on a thread that
  // does not belong to nghttp2response as it could lead to concurrency issues
  // so always post the work to send response to the IoService.
  http_context.response->SubmitWorkOnIoService(
      [response = http_context.response]() { response->Send(); });
}

void Http2Server::OnHttp2Cleanup(Uuid activity_id, Uuid request_id,
                                 uint32_t error_code) noexcept {
  if (error_code != 0) {
    auto request_id_str = ToString(request_id);
    SCP_DEBUG(kHttp2Server, activity_id,
              "The connection for request ID %s was closed with status code %d",
              request_id_str.c_str(), error_code);
  }

  active_requests_.Erase(request_id);
}

void Http2Server::OnHttp2CleanupOfRoutedRequest(Uuid activity_id,
                                                Uuid request_id,
                                                uint32_t error_code) noexcept {
  if (error_code != 0) {
    auto request_id_str = ToString(request_id);
    SCP_DEBUG(kHttp2Server, activity_id,
              "The connection for request ID %s was closed with status code %d",
              request_id_str.c_str(), error_code);
  }
}

}  // namespace google::scp::core
