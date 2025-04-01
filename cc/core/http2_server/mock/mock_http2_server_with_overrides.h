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

#include "cc/core/http2_server/src/http2_server.h"

namespace privacy_sandbox::pbs_common {

class MockHttp2ServerWithOverrides : public Http2Server {
 public:
  MockHttp2ServerWithOverrides(
      std::string& host_address, std::string& port,
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<AuthorizationProxyInterface> authorization_proxy,
      std::shared_ptr<AuthorizationProxyInterface> aws_authorization_proxy,
      const std::shared_ptr<ConfigProviderInterface>& config_provider = nullptr,
      google::scp::core::MetricRouter* metric_router = nullptr)
      : Http2Server(host_address, port, 2 /* thread_pool_size */,
                    async_executor, authorization_proxy,
                    aws_authorization_proxy, config_provider,
                    Http2ServerOptions(), metric_router) {}

  void OnHttp2Response(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http_context,
      RequestTargetEndpointType request_destination_type) noexcept override {
    if (on_http2_response_mock_) {
      on_http2_response_mock_(http_context, request_destination_type);
    }
    Http2Server::OnHttp2Response(http_context, request_destination_type);
  }

  void OnAuthorizationCallback(
      AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
          authorization_context,
      google::scp::core::common::Uuid& request_id,
      const std::shared_ptr<Http2SynchronizationContext>& sync_context) noexcept
      override {
    Http2Server::OnAuthorizationCallback(authorization_context, request_id,
                                         sync_context);
  }

  void HandleHttp2Request(
      AsyncContext<NgHttp2Request, NgHttp2Response>& http2_context,
      HttpHandler& http_handler) noexcept override {
    if (handle_http2_request_mock_) {
      return handle_http2_request_mock_(http2_context, http_handler);
    }
    return Http2Server::HandleHttp2Request(http2_context, http_handler);
  }

  void OnHttp2PendingCallback(
      google::scp::core::ExecutionResult execution_result,
      const google::scp::core::common::Uuid& request_id) noexcept override {
    Http2Server::OnHttp2PendingCallback(execution_result, request_id);
  }

  void OnHttp2Cleanup(const Http2SynchronizationContext& sync_context,
                      uint32_t error_code) noexcept override {
    Http2Server::OnHttp2Cleanup(sync_context, error_code);
  }

  google::scp::core::common::ConcurrentMap<
      std::string, std::shared_ptr<google::scp::core::common::ConcurrentMap<
                       HttpMethod, HttpHandler>>>&
  GetRegisteredResourceHandlers() {
    return resource_handlers_;
  }

  google::scp::core::common::ConcurrentMap<
      google::scp::core::common::Uuid,
      std::shared_ptr<Http2SynchronizationContext>,
      google::scp::core::common::UuidCompare>&
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
}  // namespace privacy_sandbox::pbs_common
