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

using google::scp::core::common::RetryStrategy;
using google::scp::core::common::RetryStrategyType;
using std::make_unique;
using std::shared_ptr;

namespace google::scp::core {
HttpClient::HttpClient(shared_ptr<AsyncExecutorInterface>& async_executor,
                       RetryStrategyType retry_strategy_type,
                       TimeDuration time_duraton_ms, size_t total_retries)
    : http_connection_pool_(make_unique<HttpConnectionPool>(async_executor)),
      operation_dispatcher_(
          async_executor,
          RetryStrategy(retry_strategy_type, time_duraton_ms, total_retries)) {}

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
  shared_ptr<HttpConnection> http_connection;
  operation_dispatcher_.Dispatch<AsyncContext<HttpRequest, HttpResponse>>(
      http_context,
      [this, http_connection](
          AsyncContext<HttpRequest, HttpResponse>& http_context) mutable {
        auto execution_result = http_connection_pool_->GetConnection(
            http_context.request->path, http_connection);
        if (!execution_result.Successful()) {
          return execution_result;
        }

        return http_connection->Execute(http_context);
      });

  return SuccessExecutionResult();
}
}  // namespace google::scp::core
