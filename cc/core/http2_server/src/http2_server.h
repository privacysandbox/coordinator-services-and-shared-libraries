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

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <nghttp2/asio_http2_server.h>

#include "cc/core/common/concurrent_map/src/concurrent_map.h"
#include "cc/core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/http2_server/src/http2_request.h"
#include "cc/core/http2_server/src/http2_response.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/provider.h"

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
        private_key_file(std::move(private_key_file)),
        certificate_chain_file(std::move(certificate_chain_file)),
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
      std::shared_ptr<AuthorizationProxyInterface> aws_authorization_proxy,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider =
          nullptr,
      Http2ServerOptions options = Http2ServerOptions(),
      MetricRouter* metric_router = nullptr)
      : host_address_(host_address),
        port_(port),
        thread_pool_size_(thread_pool_size),
        is_running_(false),
        authorization_proxy_(authorization_proxy),
        aws_authorization_proxy_(aws_authorization_proxy),
        config_provider_(config_provider),
        otel_server_metrics_enabled_(false),
        async_executor_(async_executor),
        operation_dispatcher_(
            async_executor,
            core::common::RetryStrategy(options.retry_strategy_options)),
        use_tls_(options.use_tls),
        private_key_file_(*options.private_key_file),
        certificate_chain_file_(*options.certificate_chain_file),
        tls_context_(boost::asio::ssl::context::sslv23),
        metric_router_(metric_router) {}

  ~Http2Server();

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
    /// Time for entry point of the request.
    std::chrono::time_point<std::chrono::steady_clock> entry_time;
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
  /**
   * @brief Handles the incoming nghttp2 native request and response. This is
   * the first function to receive the HTTP request. It initializes the
   * asynchronous context to store the request and response, binds the
   * OnHttp2Response callback, and creates a synchronous context to track active
   * requests. Finally, it sets up the request handler. The request is then
   * forwarded for further processing in RouteOrHandleHttp2Request.
   *
   * @param request The nghttp2 server request object.
   * @param response The nghttp2 server response object.
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
   * @param sync_context The sync context associated with the http operation.
   * @param error_code The error code that the connection/stream was closed
   * with.
   */
  virtual void OnHttp2Cleanup(const Http2SynchronizationContext& sync_context,
                              uint32_t error_code) noexcept;

  /**
   * @brief Handles the processing of an HTTP2 request. This function retrieves
   * the synchronization context adds details to it. It also creates an
   * authorization context to manage request authorization (dispatching
   * asynchronously).
   *
   * Additionally, this function sets up key callbacks:
   * - `SetOnRequestBodyDataReceivedCallback` triggers `OnHttp2PendingCallback`
   * when PBS receives the request body data.
   * - `SetOnCloseCallback` triggers `OnHttp2Cleanup` when PBS is finalizing the
   * request and sending the response back to the client (closing
   * connection/stream).
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
   * @brief Called upon the completion of any HTTP2 internal callback. This
   * function is triggered when PBS receives data or when
   * OnAuthorizationCallback completes. Using the synchronization context, it
   * manages the request flow by updating `pending_callbacks` to reflect the
   * current state.
   *
   * This function also sets up the context required for request processing
   * (e.g., budget consumption) and initializes the `OnHttp2Response` callback
   * to handle the final response.
   * @param execution_result The execution result of the callback.
   * @param request_id The request id associated with the operation.
   */
  virtual void OnHttp2PendingCallback(ExecutionResult execution_result,
                                      const common::Uuid& request_id) noexcept;

  // The host address to run the http server on.
  std::string host_address_;

  // The port of the http server.
  std::string port_;

  // The ngHttp2 http server instance.
  nghttp2::asio_http2::server::http2 http2_server_;

  // The total http server thread pool size.
  size_t thread_pool_size_;

  // Registry of all the paths and handlers.
  common::ConcurrentMap<
      std::string,
      std::shared_ptr<common::ConcurrentMap<HttpMethod, HttpHandler>>>
      resource_handlers_;

  // Registry of all the active requests.
  common::ConcurrentMap<common::Uuid,
                        std::shared_ptr<Http2SynchronizationContext>,
                        common::UuidCompare>
      active_requests_;

  // Indicates whether the http server is running.
  std::atomic<bool> is_running_;

  // An instance to the authorization proxy.
  std::shared_ptr<AuthorizationProxyInterface> authorization_proxy_;

  std::shared_ptr<AuthorizationProxyInterface> aws_authorization_proxy_;

  // An instance of the config provider.
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  // Feature flag for otel server metrics
  bool otel_server_metrics_enabled_;

  // An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  // An instance of the operation dispatcher.
  common::OperationDispatcher operation_dispatcher_;

  // Whether to use TLS.
  bool use_tls_;

  // The path and filename to the server private key file.
  std::string private_key_file_;

  // The path and filename of the server certificate chain file.
  std::string certificate_chain_file_;

  // The TLS context of the server.
  boost::asio::ssl::context tls_context_;

 private:
  /**
   * Initializes the OpenTelemetry metrics collection system. This function
   * sets up the necessary configurations and resources for capturing and
   * exporting metrics.
   *
   * @return ExecutionResult indicating the success or failure of the
   * initialization process.
   *
   * @noexcept This function guarantees not to throw exceptions.
   */
  ExecutionResult OtelMetricInit() noexcept;

  /**
   * Records the server latency for a given activity and request. It measures
   * the latency of a request coming to the PBS server (OnHttp2Request) until
   * the request is fully complete (OnHttp2Cleanup).
   *
   * @param sync_context The sync context associated with the http operation.
   */
  void RecordServerLatency(const Http2SynchronizationContext& sync_context);

  /**
   * Records the size of the request body (uncompressed) sent by the client.
   *
   * @param http_context The http context containing the request and response
   * objects.
   */
  void RecordRequestBodySize(
      const AsyncContext<NgHttp2Request, NgHttp2Response>& http_context);

  /**
   * Records the size of the response body (uncompressed) when request is
   * complete when complete (OnHttp2Response) and sent back to the client.
   *
   * @param http_context The http context containing the request and response
   * objects.
   */
  void RecordResponseBodySize(
      const AsyncContext<NgHttp2Request, NgHttp2Response>& http_context);

  /**
   * Callback function used by OpenTelemetry to observe the number of active
   * Http2 requests.
   *
   * This function is passed as a callback to an OTel ObservableInstrument,
   * which monitors metrics related to active requests on the server.
   *
   * @param observer_result The result object used to observe and report active
   * request counts.
   * @param self_ptr A pointer to the HTTP/2 server instance, used to access
   * request data.
   */
  static void ObserveActiveRequestsCallback(
      opentelemetry::metrics::ObserverResult observer_result,
      Http2Server* self_ptr);

  /**
   * Sets the OpenTelemetry labels
   *
   * @param http_context The http context containing the request and response
   * objects.
   *
   * @return OpenTelemetry labels.
   */
  absl::flat_hash_map<absl::string_view, std::string> GetOtelMetricLabels(
      const AsyncContext<NgHttp2Request, NgHttp2Response>& http_context);

  // An instance of metric router which will provide APIs to create metrics.
  MetricRouter* metric_router_;

  // OpenTelemetry Meter used for creating and managing metrics.
  std::shared_ptr<opentelemetry::metrics::Meter> meter_;

  // OpenTelemetry Instrument for measuring req-response latency.
  std::shared_ptr<opentelemetry::metrics::Histogram<double>>
      server_request_duration_;

  // OpenTelemetry Instrument for active Http server requests.
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      active_requests_instrument_;

  // OpenTelemetry Instrument for measuring request body size (uncompressed).
  std::shared_ptr<opentelemetry::metrics::Histogram<uint64_t>>
      server_request_body_size_;

  // OpenTelemetry Instrument for measuring response body size (uncompressed).
  std::shared_ptr<opentelemetry::metrics::Histogram<uint64_t>>
      server_response_body_size_;
};

}  // namespace google::scp::core
