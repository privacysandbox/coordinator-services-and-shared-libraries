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

#include <nghttp2/asio_http2_server.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/http_request_route_resolver_interface.h"
#include "core/interface/http_request_router_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "http2_request.h"
#include "http2_response.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

namespace google::scp::core {

class Http2ServerOptions {
 public:
  Http2ServerOptions()
      : use_tls(false),
        private_key_file(std::make_shared<std::string>()),
        certificate_chain_file(std::make_shared<std::string>()),
        retry_strategy_options(
            common::RetryStrategyOptions(common::RetryStrategyType::Exponential,
                                         kHttpServerRetryStrategyDelayInMs,
                                         kDefaultRetryStrategyMaxRetries)) {}

  Http2ServerOptions(
      bool use_tls, std::shared_ptr<std::string> private_key_file,
      std::shared_ptr<std::string> certificate_chain_file,
      common::RetryStrategyOptions retry_strategy_options =
          common::RetryStrategyOptions(common::RetryStrategyType::Exponential,
                                       kHttpServerRetryStrategyDelayInMs,
                                       kDefaultRetryStrategyMaxRetries))
      : use_tls(use_tls),
        private_key_file(move(private_key_file)),
        certificate_chain_file(move(certificate_chain_file)),
        retry_strategy_options(retry_strategy_options) {}

  /// Whether to use TLS.
  const bool use_tls;
  /// The path and filename to the server private key file.
  const std::shared_ptr<std::string> private_key_file;
  /// The path and filename of the server certificate chain file.
  const std::shared_ptr<std::string> certificate_chain_file;
  /// Retry strategy options.
  const common::RetryStrategyOptions retry_strategy_options;

 private:
  static constexpr TimeDuration kHttpServerRetryStrategyDelayInMs = 31;
};

/*! @copydoc HttpServerInterface
 */
class Http2Server : public HttpServerInterface {
 public:
  Http2Server(
      std::string& host_address, std::string& port, size_t thread_pool_size,
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<AuthorizationProxyInterface>& authorization_proxy,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client =
          nullptr,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider =
          nullptr,
      Http2ServerOptions options = Http2ServerOptions())
      : host_address_(host_address),
        port_(port),
        thread_pool_size_(thread_pool_size),
        is_running_(false),
        authorization_proxy_(authorization_proxy),
        metric_client_(metric_client),
        config_provider_(config_provider),
        aggregated_metric_interval_ms_(kDefaultAggregatedMetricIntervalMs),
        async_executor_(async_executor),
        operation_dispatcher_(
            async_executor,
            core::common::RetryStrategy(options.retry_strategy_options)),
        use_tls_(options.use_tls),
        private_key_file_(*options.private_key_file),
        certificate_chain_file_(*options.certificate_chain_file),
        tls_context_(boost::asio::ssl::context::sslv23),
        request_routing_enabled_(false),
        site_based_auth_enabled_(false) {}

  // Construct HTTP Server with Request Routing capabilities.
  Http2Server(
      std::string& host_address, std::string& port, size_t thread_pool_size,
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<AuthorizationProxyInterface>& authorization_proxy,
      std::shared_ptr<HttpRequestRouterInterface>& request_router,
      std::shared_ptr<HttpRequestRouteResolverInterface>&
          request_route_resolver,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      Http2ServerOptions options = Http2ServerOptions())
      : Http2Server(host_address, port, thread_pool_size, async_executor,
                    authorization_proxy, metric_client, config_provider,
                    options) {
    request_router_ = request_router;
    request_route_resolver_ = request_route_resolver;
  }

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult RegisterResourceHandler(
      HttpMethod http_method, std::string& path,
      HttpHandler& handler) noexcept override;

  /**
   * @brief This context is used for the synchronization between two callbacks.
   * The authorization proxy callback and the data receive callback from the
   * wire. We should never wait for the request body to be sent to us since the
   * caller can easily send multiple requests with huge amount of data on the
   * body. If the authorization validation happens earlier than the data being
   * ready the request can be terminated immediately.
   */
  struct Http2SynchronizationContext {
    /// Total pending callbacks.
    std::atomic<size_t> pending_callbacks;
    /// Indicates whether any callback has failed.
    std::atomic<bool> failed;
    /// A copy of the original http2 context.
    AsyncContext<NgHttp2Request, NgHttp2Response> http2_context;
    /// A copy of the http handler of the request.
    HttpHandler http_handler;
  };

  /**
   * @brief Request is either destined to a remote endpoint or is handled
   * locally. This structure encapsulates the info. Initially, the information
   * is unknown until the routing info is determined for a request.
   */
  enum class RequestTargetEndpointType {
    /// Default state
    Unknown,
    Local,
    Remote,
  };

 protected:
  /// Init http_error_metrics_ instance.
  virtual ExecutionResult MetricInit() noexcept;
  /// Run http_error_metrics_ instance.
  virtual ExecutionResult MetricRun() noexcept;
  /// Stop http_error_metrics_ instance.
  virtual ExecutionResult MetricStop() noexcept;

  /**
   * @brief A handler for ng2 native request response and converting them to
   * Http2Request and Http2Response behind the scenes.
   *
   * @param request The nghttp2 request.
   * @param response The nghttp2 response.
   */
  virtual void OnHttp2Request(
      const nghttp2::asio_http2::server::request& request,
      const nghttp2::asio_http2::server::response& response) noexcept;

  /**
   * @brief Is called when the http request is completed and a response needs to
   * be sent.
   *
   * @param http_context The context of the hghttp2 request.
   *
   * @param request_target_type Indicates if this request needs to be forwarded
   * or is destined to be handled locally.
   */
  virtual void OnHttp2Response(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http_context,
      RequestTargetEndpointType request_destination_type) noexcept;

  /**
   * @brief Is called when the http connection/stream is closed.
   *
   * @param activity_id Correlation ID for this request.
   * @param request_id The ID of the request.
   * @param error_code The error code that the connection/stream was closed
   * with.
   */
  virtual void OnHttp2Cleanup(common::Uuid activity_id, common::Uuid request_id,
                              uint32_t error_code) noexcept;

  /**
   * @brief Is called when the http connection/stream is closed on a request
   * routed to a remote endpoint.
   *
   * @param activity_id Correlation ID for this request.
   * @param request_id The ID of the request.
   * @param error_code The error code that the connection/stream was closed
   * with.
   */
  virtual void OnHttp2CleanupOfRoutedRequest(common::Uuid activity_id,
                                             common::Uuid request_id,
                                             uint32_t error_code) noexcept;

  /**
   * @brief Decide whether to route to another instance or handle the http2
   * request on local instance.
   *
   * @param http2_context The context of the ng http2 operation.
   * @param http_handler The http handler to handle the request.
   */
  virtual void RouteOrHandleHttp2Request(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
      HttpHandler& http_handler) noexcept;

  /**
   * @brief A handler for the http2 request.
   *
   * @param http2_context The context of the ng http2 operation.
   * @param http_handler The http handler to handle the request.
   */
  virtual void HandleHttp2Request(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
      HttpHandler& http_handler) noexcept;

  /**
   * @brief The callback that is called after authorization proxy evaluates
   * the http context authorization.
   *
   * @param authorization_context The authorization context of the operation.
   * @param request_id The id of the request.
   * @param sync_context The sync context associated with the http operation.
   */
  virtual void OnAuthorizationCallback(
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
          authorization_context,
      common::Uuid& request_id,
      const std::shared_ptr<Http2SynchronizationContext>&
          sync_context) noexcept;

  /**
   * @brief Is called when any of the http2 internal callbacks are complete.
   *
   * @param execution_result The execution result of the callback.
   * @param request_id The request id associated with the operation.
   */
  virtual void OnHttp2PendingCallback(ExecutionResult execution_result,
                                      const common::Uuid& request_id) noexcept;

  /**
   * @brief Is called when the data is obtained on the http2 request and is
   * ready to be routed. Routing is done in this function to the endpoint.
   *
   * @param context context of the request to be routed.
   * @param endpoint_info The endpoint to route the request to.
   * @param request_body_received_result Result of obtaining request data on the
   * connection.
   */
  virtual void OnHttp2RequestDataObtainedRoutedRequest(
      AsyncContext<NgHttp2Request, NgHttp2Response>& context,
      const RequestRouteEndpointInfo& endpoint_info,
      ExecutionResult request_body_received_result) noexcept;

  /**
   * @brief Is called when routing is completed with response.
   *
   * @param http2_context original http2 context for which routing was
   * requested.
   * @param context context of the routing request
   */
  virtual void OnRoutingResponseReceived(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
      AsyncContext<HttpRequest, HttpResponse>& context) noexcept;

  /**
   * @brief Is the request forwarding feature enabled?
   *
   * @return true
   * @return false
   */
  bool IsRequestForwardingEnabled() const;

  /// The host address to run the http server on.
  std::string host_address_;

  /// The port of the http server.
  std::string port_;

  /// The ngHttp2 http server instance.
  nghttp2::asio_http2::server::http2 http2_server_;

  /// The total http server thread pool size.
  size_t thread_pool_size_;

  /// Registry of all the paths and handlers.
  common::ConcurrentMap<
      std::string,
      std::shared_ptr<common::ConcurrentMap<HttpMethod, HttpHandler>>>
      resource_handlers_;

  /// Registry of all the active requests.
  common::ConcurrentMap<common::Uuid,
                        std::shared_ptr<Http2SynchronizationContext>,
                        common::UuidCompare>
      active_requests_;

  /// Indicates whether the http server is running.
  std::atomic<bool> is_running_;

  /// An instance to the authorization proxy.
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy_;

  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  /// An instance of the config provider.
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// The time interval for metrics aggregation.
  TimeDuration aggregated_metric_interval_ms_;

  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// The AggregateMetric instance for http request metrics.
  std::shared_ptr<cpio::AggregateMetricInterface> http_request_metrics_;

  /// An instance of the operation dispatcher.
  common::OperationDispatcher operation_dispatcher_;

  /// Whether to use TLS.
  bool use_tls_;

  /// The path and filename to the server private key file.
  std::string private_key_file_;

  /// The path and filename of the server certificate chain file.
  std::string certificate_chain_file_;

  /// The TLS context of the server.
  boost::asio::ssl::context tls_context_;

  /// @brief Router to forward a request to a remote instance if needed.
  std::shared_ptr<HttpRequestRouterInterface> request_router_;

  /// @brief Resolves target route of a request.
  std::shared_ptr<HttpRequestRouteResolverInterface> request_route_resolver_;

  /// @brief enables disables request routing.
  bool request_routing_enabled_;

  /// @brief enables adtech site based authorization of PBS requests.
  bool site_based_auth_enabled_;
};
}  // namespace google::scp::core
