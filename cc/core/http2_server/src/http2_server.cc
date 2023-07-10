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

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nghttp2/asio_http2_server.h>

#include "core/authorization_proxy/src/error_codes.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/aggregate_metric_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "cpio/client_providers/metric_client_provider/src/utils/aggregate_metric.h"
#include "public/core/interface/execution_result.h"

#include "http2_utils.h"

using boost::asio::ssl::context;
using boost::system::error_code;
using boost::system::errc::success;
using google::scp::core::common::ConcurrentMap;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::errors::GetErrorHttpStatusCode;
using google::scp::core::errors::HttpStatusCode;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::client_providers::AggregateMetric;
using google::scp::cpio::client_providers::AggregateMetricInterface;
using google::scp::cpio::client_providers::kCountSecond;
using google::scp::cpio::client_providers::MetricDefinition;
using google::scp::cpio::client_providers::MetricLabelsBase;
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
static constexpr size_t kMetricsTimeDurationMs = 1000;
static constexpr char kHttpErrorStatus[] = "HttpErrorStatus";
static constexpr char kHttpError4[] = "400-499";
static constexpr char kHttpError5[] = "500-599";

static const set<HttpStatusCode> kHttpError4Map = {
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

static const set<HttpStatusCode> kHttpError5Map = {
    HttpStatusCode::INTERNAL_SERVER_ERROR,
    HttpStatusCode::NOT_IMPLEMENTED,
    HttpStatusCode::BAD_GATEWAY,
    HttpStatusCode::SERVICE_UNAVAILABLE,
    HttpStatusCode::GATEWAY_TIMEOUT,
    HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED};

namespace google::scp::core {
ExecutionResult Http2Server::MetricInit() noexcept {
  auto metric_name = make_shared<MetricName>(kHttpErrorStatus);
  auto metric_unit = make_shared<MetricUnit>(kCountSecond);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(kHttp2Server);
  metric_info->labels =
      make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  vector<string> event_list = {kHttpError4, kHttpError5};
  http_error_metrics_ = make_shared<AggregateMetric>(
      async_executor_, metric_client_, metric_info, kMetricsTimeDurationMs,
      make_shared<vector<string>>(event_list));
  return http_error_metrics_->Init();
}

ExecutionResult Http2Server::MetricRun() noexcept {
  return http_error_metrics_->Run();
}

ExecutionResult Http2Server::MetricStop() noexcept {
  return http_error_metrics_->Stop();
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
      ERROR(kHttp2Server, kZeroUuid, kZeroUuid, execution_result,
            "Failed to initialize TLS context.");
      return execution_result;
    }

    if (nghttp2_error_code.value() != success) {
      auto execution_result = FailureExecutionResult(
          core::errors::SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT);
      ERROR(kHttp2Server, kZeroUuid, kZeroUuid, execution_result,
            "Failed to initialize TLS context with error code: %d",
            nghttp2_error_code.value());
      return execution_result;
    }
  }

  if (metric_client_) {
    return MetricInit();
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
    http2_server_.join();
  } catch (...) {
    // Doing the best to stop, ignore otherwise.
  }

  if (metric_client_) {
    return MetricInit();
  }
  return SuccessExecutionResult();
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
  auto activity_id = Uuid::GenerateUuid();
  AsyncContext<NgHttp2Request, NgHttp2Response> http2_context(
      make_shared<NgHttp2Request>(request),
      bind(&Http2Server::OnHttp2Response, this, _1), activity_id);
  http2_context.parent_activity_id = activity_id;
  http2_context.response = make_shared<NgHttp2Response>(response);
  http2_context.response->headers = make_shared<core::HttpHeaders>();

  auto request_id = http2_context.request->id;
  http2_context.response->on_closed =
      bind(&Http2Server::OnHttp2Cleanup, this, activity_id, request_id, _1);

  DEBUG_CONTEXT(kHttp2Server, http2_context, "Received a http2 request");

  auto execution_result = http2_context.request->UnwrapNgHttp2Request();
  if (!execution_result.Successful()) {
    http2_context.result = execution_result;
    http2_context.Finish();
    return;
  }

  // Check if path is registered
  shared_ptr<ConcurrentMap<HttpMethod, HttpHandler>> resource_handler;
  execution_result =
      resource_handlers_.Find(*http2_context.request->path, resource_handler);
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

  HandleHttp2Request(http2_context, http_handler);
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

  auto request_id = http2_context.request->id;
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

  DEBUG_CONTEXT(
      kHttp2Server, http2_context,
      "Sending authorization request for request with path: %s, "
      "claimed identity: %s",
      http2_context.request->path->c_str(),
      authorization_request->authorization_metadata.claimed_identity.c_str());

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_context(authorization_request,
                            bind(&Http2Server::OnAuthorizationCallback, this,
                                 _1, request_id, sync_context),
                            http2_context.activity_id);

  operation_dispatcher_.Dispatch<
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>>(
      authorization_context,
      [authorization_proxy = authorization_proxy_](
          AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
              authorization_context) {
        return authorization_proxy->Authorize(authorization_context);
      });

  sync_context->http2_context.request->on_request_body_received =
      bind(&Http2Server::OnHttp2PendingCallback, this, _1, request_id);
}

void Http2Server::OnAuthorizationCallback(
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
        authorization_context,
    Uuid& request_id,
    const shared_ptr<Http2SynchronizationContext>& sync_context) noexcept {
  if (!authorization_context.result.Successful()) {
    DEBUG_CONTEXT(kHttp2Server, authorization_context, "Authorization failed.");
  } else {
    sync_context->http2_context.request->auth_context.authorized_domain =
        authorization_context.response->authorized_metadata.authorized_domain;
  }

  OnHttp2PendingCallback(authorization_context.result, request_id);
}

void Http2Server::OnHttp2PendingCallback(
    ExecutionResult& callback_execution_result, Uuid& request_id) noexcept {
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
  http_context.request =
      static_pointer_cast<HttpRequest>(sync_context->http2_context.request);
  http_context.response =
      static_pointer_cast<HttpResponse>(sync_context->http2_context.response);
  http_context.callback =
      [this, http2_context = sync_context->http2_context](
          AsyncContext<HttpRequest, HttpResponse>& http_context) mutable {
        http2_context.result = http_context.result;
        OnHttp2Response(http2_context);
      };

  execution_result = sync_context->http_handler(http_context);
  if (!execution_result.Successful()) {
    sync_context->http2_context.result = execution_result;
    sync_context->http2_context.Finish();
    return;
  }
}

void Http2Server::OnHttp2Response(
    AsyncContext<NgHttp2Request, NgHttp2Response>& http_context) noexcept {
  http_context.response->code = HttpStatusCode::OK;
  if (!http_context.result.Successful()) {
    auto error_code = GetErrorHttpStatusCode(http_context.result.status_code);
    http_context.response->code = error_code;
    ERROR_CONTEXT(
        kHttp2Server, http_context, http_context.result,
        "http2 request finished with error. http status code: %d",
        static_cast<typename std::underlying_type<HttpStatusCode>::type>(
            http_context.response->code));
    if (metric_client_) {
      auto code_pair = kHttpError4Map.find(error_code);
      if (code_pair != kHttpError4Map.end()) {
        http_error_metrics_->Increment(kHttpError4);
      }
      code_pair = kHttpError5Map.find(error_code);
      if (code_pair != kHttpError5Map.end()) {
        http_error_metrics_->Increment(kHttpError5);
      }
    }
  } else {
    DEBUG_CONTEXT(kHttp2Server, http_context, "http2 request finished.");
  }

  http_context.response->Send();
}

void Http2Server::OnHttp2Cleanup(Uuid activity_id, Uuid request_id,
                                 uint32_t error_code) noexcept {
  if (error_code != 0) {
    auto request_id_str = ToString(request_id);
    DEBUG(kHttp2Server, activity_id, activity_id,
          "The connection for request ID %s was closed with status code %d",
          request_id_str.c_str(), error_code);
  }

  active_requests_.Erase(request_id);
}
}  // namespace google::scp::core
