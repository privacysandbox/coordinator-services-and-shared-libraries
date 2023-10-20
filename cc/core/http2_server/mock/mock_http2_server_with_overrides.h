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

#include <memory>
#include <string>

#include "core/http2_server/src/http2_server.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

namespace google::scp::core::http2_server::mock {

class MockHttp2ServerWithOverrides : public core::Http2Server {
 public:
  MockHttp2ServerWithOverrides(
      std::string& host_address, std::string& port,
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<AuthorizationProxyInterface>& authorization_proxy,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider =
          nullptr)
      : core::Http2Server(host_address, port, 2 /* thread_pool_size */,
                          async_executor, authorization_proxy, metric_client,
                          config_provider) {}

  // Construct HTTP Server with Request Routing capabilities.
  MockHttp2ServerWithOverrides(
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

  ExecutionResult MetricInit() noexcept { return SuccessExecutionResult(); }

  ExecutionResult MetricRun() noexcept { return SuccessExecutionResult(); }

  ExecutionResult MetricStop() noexcept { return SuccessExecutionResult(); }

  void OnHttp2Response(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http_context,
      RequestTargetEndpointType request_destination_type) noexcept override {
    if (on_http2_response_mock_) {
      on_http2_response_mock_(http_context, request_destination_type);
    }
    core::Http2Server::OnHttp2Response(http_context, request_destination_type);
  }

  void OnAuthorizationCallback(
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
          authorization_context,
      common::Uuid& request_id,
      const std::shared_ptr<Http2SynchronizationContext>&
          sync_context) noexcept {
    core::Http2Server::OnAuthorizationCallback(authorization_context,
                                               request_id, sync_context);
  }

  void HandleHttp2Request(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
      HttpHandler& http_handler) noexcept {
    if (handle_http2_request_mock_) {
      return handle_http2_request_mock_(http2_context, http_handler);
    }
    return core::Http2Server::HandleHttp2Request(http2_context, http_handler);
  }

  void RouteOrHandleHttp2Request(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
      HttpHandler& http_handler) noexcept {
    return core::Http2Server::RouteOrHandleHttp2Request(http2_context,
                                                        http_handler);
  }

  void OnHttp2RequestDataObtainedRoutedRequest(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
      const RequestRouteEndpointInfo endpoint_info,
      ExecutionResult request_body_received_result) noexcept {
    return core::Http2Server::OnHttp2RequestDataObtainedRoutedRequest(
        http2_context, endpoint_info, request_body_received_result);
  }

  void OnHttp2PendingCallback(ExecutionResult& execution_result,
                              common::Uuid& request_id) noexcept {
    core::Http2Server::OnHttp2PendingCallback(execution_result, request_id);
  }

  void OnHttp2Cleanup(common::Uuid activity_id, common::Uuid request_id,
                      uint32_t error_code) noexcept {
    core::Http2Server::OnHttp2Cleanup(activity_id, request_id, error_code);
  }

  common::ConcurrentMap<
      std::string,
      std::shared_ptr<common::ConcurrentMap<HttpMethod, HttpHandler>>>&
  GetRegisteredResourceHandlers() {
    return resource_handlers_;
  }

  common::ConcurrentMap<common::Uuid,
                        std::shared_ptr<Http2SynchronizationContext>,
                        common::UuidCompare>&
  GetActiveRequests() {
    return active_requests_;
  }

  std::function<void(AsyncContext<NgHttp2Request, NgHttp2Response>&,
                     HttpHandler& http_handler)>
      handle_http2_request_mock_;

  std::function<void(AsyncContext<NgHttp2Request, NgHttp2Response>&,
                     RequestTargetEndpointType)>
      on_http2_response_mock_;
};
}  // namespace google::scp::core::http2_server::mock
