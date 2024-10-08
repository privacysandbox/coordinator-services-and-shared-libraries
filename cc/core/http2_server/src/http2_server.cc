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

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/authorization_service/src/error_codes.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_server/src/error_codes.h"
#include "core/http2_server/src/http2_utils.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/errors.h"
#include "core/interface/metrics_def.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "http2_utils.h"
#include "nlohmann/json.hpp"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/view/view.h"
#include "opentelemetry/sdk/metrics/view/view_factory.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"
#include "public/cpio/utils/metric_aggregation/src/aggregate_metric.h"

namespace google::scp::core {
namespace {

using ::absl::StrCat;
using ::boost::asio::ssl::context;
using ::boost::system::error_code;
using ::boost::system::errc::success;
using ::google::scp::core::common::ConcurrentMap;
using ::google::scp::core::common::kZeroUuid;
using ::google::scp::core::common::Uuid;
using ::google::scp::core::errors::GetErrorHttpStatusCode;
using ::google::scp::core::errors::HttpStatusCode;
using ::google::scp::core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN;
using ::google::scp::core::utils::Base64Decode;
using ::google::scp::core::utils::PadBase64Encoding;
using ::google::scp::cpio::AggregateMetric;
using ::google::scp::cpio::AggregateMetricInterface;
using ::google::scp::cpio::kCountSecond;
using ::google::scp::cpio::MetricDefinition;
using ::google::scp::cpio::MetricLabels;
using ::google::scp::cpio::MetricLabelsBase;
using ::google::scp::cpio::MetricName;
using ::google::scp::cpio::MetricUnit;
using ::nghttp2::asio_http2::server::configure_tls_context_easy;
using ::nghttp2::asio_http2::server::request;
using ::nghttp2::asio_http2::server::response;

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

ExecutionResult SetSyncContext(
    const AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
    const HttpHandler& http_handler, bool otel_server_metrics_enabled,
    common::ConcurrentMap<
        common::Uuid, std::shared_ptr<Http2Server::Http2SynchronizationContext>,
        common::UuidCompare>& active_requests,
    std::shared_ptr<Http2Server::Http2SynchronizationContext>& sync_context) {
  ExecutionResult execution_result;
  if (otel_server_metrics_enabled) {
    execution_result =
        active_requests.Find(http2_context.request->id, sync_context);
    if (execution_result.Successful()) {
      sync_context->pending_callbacks =
          2;  // 1 for authorization, 1 for body data.
      sync_context->http2_context = http2_context;
      sync_context->http_handler = http_handler;
      sync_context->failed = false;
    } else {
      SCP_ERROR_CONTEXT(kHttp2Server, http2_context, execution_result,
                        "[HandleHttp2Request] Cannot find the sync context in "
                        "the active requests map!");
    }
  } else {
    sync_context = std::make_shared<Http2Server::Http2SynchronizationContext>();
    sync_context->pending_callbacks =
        2;  // 1 for authorization, 1 for body data.
    sync_context->http2_context = http2_context;
    sync_context->http_handler = http_handler;
    sync_context->failed = false;

    auto context_pair = std::make_pair(http2_context.request->id, sync_context);
    execution_result = active_requests.Insert(context_pair, sync_context);
    if (!execution_result.Successful()) {
      SCP_ERROR_CONTEXT(kHttp2Server, http2_context, execution_result,
                        "[HandleHttp2Request] Cannot insert the sync context "
                        "to the active requests map!");
    }
  }
  return execution_result;
}
}  // namespace

ExecutionResult Http2Server::MetricInit() noexcept {
  auto metric_name = std::make_shared<MetricName>(kMetricNameHttpRequest);
  auto metric_unit = std::make_shared<MetricUnit>(kCountSecond);
  auto metric_info =
      std::make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(kHttp2Server);
  metric_info->labels =
      std::make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  std::vector<std::string> event_list = {kMetricEventHttpUnableToResolveRoute,
                                         kMetricEventHttp2xxLocal,
                                         kMetricEventHttp4xxLocal,
                                         kMetricEventHttp5xxLocal,
                                         kMetricEventHttp2xxForwarded,
                                         kMetricEventHttp4xxForwarded,
                                         kMetricEventHttp5xxForwarded};
  http_request_metrics_ = std::make_shared<AggregateMetric>(
      async_executor_, metric_client_, metric_info,
      aggregated_metric_interval_ms_,
      std::make_shared<std::vector<std::string>>(event_list));
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

  bool adtech_site_authorized_domain_enabled = false;
  if (config_provider_ && config_provider_
                              ->Get(kPBSAdtechSiteAsAuthorizedDomain,
                                    adtech_site_authorized_domain_enabled)
                              .Successful()) {
    adtech_site_authorized_domain_enabled_ =
        adtech_site_authorized_domain_enabled;
  }

  // Otel metrics setup.
  auto histogram_instrument_selector =
      std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
          opentelemetry::sdk::metrics::InstrumentType::kHistogram,
          kServerDurationMetric, "s");

  auto histogram_meter_selector =
      std::make_unique<opentelemetry::sdk::metrics::MeterSelector>(
          kServerDurationMetric, "v1.0", "");

  auto histogram_aggregation_config = std::make_shared<
      opentelemetry::sdk::metrics::HistogramAggregationConfig>();

  // Define explicit bucket boundaries for server latency.
  histogram_aggregation_config->boundaries_ =
      std::vector<double>{0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25,
                          0.5,   0.75, 1,     2.5,  5,     7.5, 10};

  std::unique_ptr<opentelemetry::sdk::metrics::View> histogram_view =
      opentelemetry::sdk::metrics::ViewFactory::Create(
          kServerDurationMetric, "Server request duration in seconds", "s",
          opentelemetry::sdk::metrics::AggregationType::kHistogram,
          histogram_aggregation_config);

  std::shared_ptr<opentelemetry::metrics::MeterProvider> meter_provider =
      opentelemetry::metrics::Provider::GetMeterProvider();

  // Add the view only when provider has been initialized properly.
  if (!dynamic_cast<opentelemetry::metrics::NoopMeterProvider*>(
          meter_provider.get())) {
    std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> sdk_provider =
        std::dynamic_pointer_cast<opentelemetry::sdk::metrics::MeterProvider>(
            meter_provider);

    sdk_provider->AddView(std::move(histogram_instrument_selector),
                          std::move(histogram_meter_selector),
                          std::move(histogram_view));
  }

  meter_ = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
      "Http2 Server");

  server_request_duration_ = meter_->CreateDoubleHistogram(
      kServerDurationMetric, "Server request duration in seconds", "s");
  active_requests_instrument_ = meter_->CreateInt64ObservableGauge(
      kActiveRequestsMetric, "Active Http server requests");
  server_request_body_size_ = meter_->CreateUInt64Histogram(
      kServerRequestBodySizeMetric,
      "Server request body size in Bytes - uncompressed", "By");
  server_response_body_size_ = meter_->CreateUInt64Histogram(
      kServerResponseBodySizeMetric,
      "Server response body size in Bytes - uncompressed", "By");
  pbs_transactions_ =
      meter_->CreateUInt64Counter(kPbsTransactionMetric, "Pbs transactions");

  active_requests_instrument_->AddCallback(
      reinterpret_cast<opentelemetry::metrics::ObservableCallbackPtr>(
          &Http2Server::ObserveActiveRequestsCallback),
      this);

  if (config_provider_) {
    config_provider_->Get(kOtelServerMetricsEnabled,
                          otel_server_metrics_enabled_);
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
      std::make_shared<ConcurrentMap<HttpMethod, HttpHandler>>();
  auto path_to_map_pair = std::make_pair(path, verb_to_handler_map);

  auto execution_result =
      resource_handlers_.Insert(path_to_map_pair, verb_to_handler_map);
  if (!execution_result.Successful()) {
    if (execution_result !=
        FailureExecutionResult(
            errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)) {
      return execution_result;
    }
  }

  auto verb_to_handler_pair = std::make_pair(http_method, handler);
  return verb_to_handler_map->Insert(verb_to_handler_pair, handler);
}

void Http2Server::OnHttp2Request(const request& request,
                                 const response& response) noexcept {
  // Timestamp entry_time = GetSteadyTimestampInNanosecondsAsClockTicks;
  std::chrono::time_point<std::chrono::steady_clock> entry_time =
      std::chrono::steady_clock::now();
  auto parent_activity_id = Uuid::GenerateUuid();
  auto http2Request = std::make_shared<NgHttp2Request>(request);
  auto request_endpoint_type = RequestTargetEndpointType::Unknown;
  if (!IsRequestForwardingEnabled()) {
    request_endpoint_type = RequestTargetEndpointType::Local;
  }

  // This is the entry point of a Http2Request.
  // The Http2Request ID that we generate here is used as the correlation ID
  // throughout the lifetime of this context and subsequent child contexts.
  AsyncContext<NgHttp2Request, NgHttp2Response> http2_context(
      http2Request,
      std::bind(&Http2Server::OnHttp2Response, this, std::placeholders::_1,
                request_endpoint_type),
      parent_activity_id, http2Request->id);

  http2_context.response = std::make_shared<NgHttp2Response>(response);
  http2_context.response->headers = std::make_shared<core::HttpHeaders>();

  if (otel_server_metrics_enabled_) {
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
  }

  SCP_DEBUG_CONTEXT(kHttp2Server, http2_context, "Received a http2 request");

  auto execution_result = http2_context.request->UnwrapNgHttp2Request();
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
      http2_context.callback =
          std::bind(&Http2Server::OnHttp2Response, this, std::placeholders::_1,
                    RequestTargetEndpointType::Remote);
      // Perform routing when request data is obtained on the connection. If the
      // connection is closed, do OnHttp2CleanupRoutedRequest.
      http2_context.request->SetOnRequestBodyDataReceivedCallback(
          std::bind(&Http2Server::OnHttp2RequestDataObtainedRoutedRequest, this,
                    http2_context, *endpoint_info, std::placeholders::_1));
      http2_context.response->SetOnCloseCallback(
          std::bind(&Http2Server::OnHttp2CleanupOfRoutedRequest, this,
                    http2_context.request->id, http2_context.request->id,
                    std::placeholders::_1));
      return;
    }
    // Rebind the callback with the updated request target type
    http2_context.callback =
        std::bind(&Http2Server::OnHttp2Response, this, std::placeholders::_1,
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
      std::static_pointer_cast<HttpRequest>(http2_context.request),
      std::bind(&Http2Server::OnRoutingResponseReceived, this, http2_context,
                std::placeholders::_1),
      http2_context);
  // The target path should reflect the forwarding endpoint.
  routing_context.request->path = std::make_shared<std::string>(
      absl::StrCat(*endpoint_info.uri, http2_context.request->handler_path));

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

  std::shared_ptr<Http2SynchronizationContext> sync_context;

  auto execution_result =
      SetSyncContext(http2_context, http_handler, otel_server_metrics_enabled_,
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
      "Sending authorization request for request with path: %s, "
      "claimed identity: %s",
      http2_context.request->handler_path.c_str(),
      authorization_request->authorization_metadata.claimed_identity.c_str());

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_context(authorization_request,
                            std::bind(&Http2Server::OnAuthorizationCallback,
                                      this, std::placeholders::_1,
                                      http2_context.request->id, sync_context),
                            http2_context);

  std::shared_ptr<AuthorizationProxyInterface>& authorization_proxy_to_use =
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
      std::bind(&Http2Server::OnHttp2Cleanup, this, http2_context.request->id,
                http2_context.request->id, std::placeholders::_1));
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
  const absl::flat_hash_map<std::string, std::string> label_kv;
  opentelemetry::context::Context context;
  server_request_body_size_->Record(http_context.request->body.length, label_kv,
                                    context);

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
    std::shared_ptr<AggregateMetricInterface> metric,
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

  // Record response body size in Bytes - response is prepared here to be sent.
  const absl::flat_hash_map<std::string, std::string> response_body_label_kv = {
      {kResponseCode,
       std::to_string(static_cast<int>(http_context.response->code))}};
  opentelemetry::context::Context context;
  server_response_body_size_->Record(http_context.response->body.length,
                                     response_body_label_kv, context);

  // Increment pbs transactions counter.
  const absl::flat_hash_map<std::string, std::string> pbs_transaction_label_kv =
      {{kResponseCode,
        std::to_string(static_cast<int>(http_context.response->code))}};

  pbs_transactions_->Add(1, pbs_transaction_label_kv);

  // Capture the shared_ptr to keep the response object alive when the work
  // actually starts executing. Do not execute response->Send() on a thread
  // that does not belong to nghttp2response as it could lead to concurrency
  // issues so always post the work to send response to the IoService.
  http_context.response->SubmitWorkOnIoService(
      [response = http_context.response]() { response->Send(); });
}

void Http2Server::OnHttp2Cleanup(Uuid activity_id, Uuid request_id,
                                 uint32_t error_code) noexcept {
  auto request_id_str = ToString(request_id);
  if (error_code != 0) {
    SCP_DEBUG(kHttp2Server, activity_id,
              "The connection for request ID %s was closed with status code %d",
              request_id_str.c_str(), error_code);
  }
  if (otel_server_metrics_enabled_) {
    RecordServerLatency(activity_id, request_id);
  }
  active_requests_.Erase(request_id);
}

void Http2Server::RecordServerLatency(const common::Uuid& activity_id,
                                      const common::Uuid& request_id) {
  auto request_id_str = ToString(request_id);
  std::shared_ptr<Http2SynchronizationContext> sync_context;
  auto execution_result = active_requests_.Find(request_id, sync_context);
  if (!execution_result.Successful()) {
    SCP_DEBUG(kHttp2Server, activity_id,
              "Could not find the Http2SynchronizationContext for the "
              "request id %s",
              request_id_str.c_str());
    return;
  }

  std::chrono::duration<double> latency =
      std::chrono::steady_clock::now() - sync_context->entry_time;
  double latency_s = latency / std::chrono::seconds(1);

  const absl::flat_hash_map<std::string, std::string> http_request_label_kv = {
      {kExecutionStatus, ExecutionStatusToString(execution_result.status)}};
  opentelemetry::context::Context context;
  server_request_duration_->Record(latency_s, http_request_label_kv, context);
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

void Http2Server::ObserveActiveRequestsCallback(
    opentelemetry::metrics::ObserverResult observer_result,
    absl::Nonnull<Http2Server*> self_ptr) {
  auto observer = std::get<
      std::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
      observer_result);
  observer->Observe(self_ptr->active_requests_.Size());
}

}  // namespace google::scp::core
