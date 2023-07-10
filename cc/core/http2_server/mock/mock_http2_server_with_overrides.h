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
#include "cpio/client_providers/metric_client_provider/mock/utils/mock_aggregate_metric.h"

namespace google::scp::core::http2_server::mock {

class MockHttp2ServerWithOverrides : public core::Http2Server {
 public:
  MockHttp2ServerWithOverrides(
      std::string& host_address, std::string& port,
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<AuthorizationProxyInterface>& authorization_proxy,
      const std::shared_ptr<
          cpio::client_providers::MetricClientProviderInterface>& metric_client)
      : core::Http2Server(host_address, port, 2 /* thread_pool_size */,
                          async_executor, authorization_proxy, metric_client) {}

  ExecutionResult MetricInit() noexcept {
    core::Http2Server::http_error_metrics_ =
        std::make_shared<cpio::client_providers::mock::MockAggregateMetric>();
    return SuccessExecutionResult();
  }

  ExecutionResult MetricRun() noexcept { return SuccessExecutionResult(); }

  ExecutionResult MetricStop() noexcept { return SuccessExecutionResult(); }

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
    core::Http2Server::HandleHttp2Request(http2_context, http_handler);
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
};
}  // namespace google::scp::core::http2_server::mock
