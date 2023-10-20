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

#include "http2_client.h"

#include <memory>

using google::scp::core::common::kZeroUuid;
using google::scp::core::common::RetryStrategy;
using google::scp::core::common::RetryStrategyType;
using std::make_unique;
using std::shared_ptr;

constexpr char kHttpClient[] = "Http2Client";

namespace google::scp::core {
HttpClient::HttpClient(shared_ptr<AsyncExecutorInterface>& async_executor,
                       HttpClientOptions options)
    : http_connection_pool_(make_unique<HttpConnectionPool>(
          async_executor, options.max_connections_per_host,
          options.http2_read_timeout_in_sec)),
      operation_dispatcher_(async_executor,
                            RetryStrategy(options.retry_strategy_options)) {}

ExecutionResult HttpClient::Init() noexcept {
  return http_connection_pool_->Init();
}

ExecutionResult HttpClient::Run() noexcept {
  return http_connection_pool_->Run();
}

ExecutionResult HttpClient::Stop() noexcept {
  return http_connection_pool_->Stop();
}

ExecutionResult HttpClient::PerformRequest(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  operation_dispatcher_.Dispatch<AsyncContext<HttpRequest, HttpResponse>>(
      http_context,
      [this](AsyncContext<HttpRequest, HttpResponse>& http_context) mutable {
        shared_ptr<HttpConnection> http_connection;
        auto execution_result = http_connection_pool_->GetConnection(
            http_context.request->path, http_connection);
        if (!execution_result.Successful()) {
          return execution_result;
        }

        SCP_DEBUG_CONTEXT(
            kHttpClient, http_context,
            "Executing request on connection %p. Retry count: %lld",
            http_connection.get(), http_context.retry_count);

        return http_connection->Execute(http_context);
      });

  return SuccessExecutionResult();
}
}  // namespace google::scp::core
