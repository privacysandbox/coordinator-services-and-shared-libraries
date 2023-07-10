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

#include <nghttp2/asio_http2_server.h>

#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/uuid/src/uuid.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/aggregate_metric_interface.h"

#include "http2_request.h"
#include "http2_response.h"

static constexpr size_t kHttpServerRetryStrategyDelayMs = 31;
static constexpr size_t kHttpServerRetryStrategyTotalRetries = 12;

namespace google::scp::core {

/*! @copydoc HttpServerInterface
 */
class Http2Server : public HttpServerInterface {
 public:
  Http2Server(std::string& host_address, std::string& port,
              size_t thread_pool_size,
              std::shared_ptr<AsyncExecutorInterface>& async_executor,
              std::shared_ptr<AuthorizationProxyInterface>& authorization_proxy,
              const std::shared_ptr<
                  cpio::client_providers::MetricClientProviderInterface>&
                  metric_client = nullptr,
              bool use_tls = false, const std::string& private_key_file = "",
              const std::string& certificate_chain_file = "")
      : host_address_(host_address),
        port_(port),
        thread_pool_size_(thread_pool_size),
        is_running_(false),
        authorization_proxy_(authorization_proxy),
        metric_client_(metric_client),
        async_executor_(async_executor),
        operation_dispatcher_(async_executor,
                              core::common::RetryStrategy(
                                  core::common::RetryStrategyType::Exponential,
                                  kHttpServerRetryStrategyDelayMs,
                                  kHttpServerRetryStrategyTotalRetries)),
        use_tls_(use_tls),
        private_key_file_(private_key_file),
        certificate_chain_file_(certificate_chain_file),
        tls_context_(boost::asio::ssl::context::sslv23) {}

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
   */
  virtual void OnHttp2Response(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http_context) noexcept;

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
   */
  virtual void OnAuthorizationCallback(
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
          authorization_context,
      common::Uuid& request_id,
      const std::shared_ptr<Http2SynchronizationContext>& sync_context =
          nullptr) noexcept;

  /**
   * @brief Is called when any of the http2 internal callbacks are complete.
   *
   * @param execution_result The execution result of the callback.
   * @param request_id The request id associated with the operation.
   */
  virtual void OnHttp2PendingCallback(ExecutionResult& execution_result,
                                      common::Uuid& request_id) noexcept;

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
  std::shared_ptr<cpio::client_providers::MetricClientProviderInterface>
      metric_client_;

  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// The AggregateMetric instance for http errors metric.
  std::shared_ptr<cpio::client_providers::AggregateMetricInterface>
      http_error_metrics_;

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
};
}  // namespace google::scp::core
