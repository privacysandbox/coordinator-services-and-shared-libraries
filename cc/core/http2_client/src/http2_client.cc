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

#include "cc/core/http2_client/src/http2_client.h"

#include <memory>
#include <string>

#include "cc/core/http2_client/src/http_client_def.h"
#include "cc/core/utils/src/http.h"
#include "opentelemetry/metrics/provider.h"

namespace privacy_sandbox::pbs_common {
using std::make_unique;
using std::shared_ptr;

constexpr char kHttpClient[] = "Http2Client";

HttpClient::HttpClient(shared_ptr<AsyncExecutorInterface>& async_executor,
                       HttpClientOptions options,
                       absl::Nullable<MetricRouter*> metric_router)
    : http_connection_pool_(make_unique<HttpConnectionPool>(
          async_executor, metric_router, options.max_connections_per_host,
          options.http2_read_timeout_in_sec)),
      operation_dispatcher_(async_executor,
                            RetryStrategy(options.retry_strategy_options)),
      metric_router_(metric_router) {
  if (metric_router_) {
    meter_ = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
        kHttpClientMeter);

    client_connection_creation_error_counter_ =
        std::static_pointer_cast<opentelemetry::metrics::Counter<uint64_t>>(
            metric_router_->GetOrCreateSyncInstrument(
                kClientConnectionCreationErrorsMetric,
                [&]() -> std::shared_ptr<
                          opentelemetry::metrics::SynchronousInstrument> {
                  return meter_->CreateUInt64Counter(
                      kClientConnectionCreationErrorsMetric,
                      "Total number of client connection creation errors");
                }));
  }
}

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
          IncrementClientConnectionCreationError(http_context);
          return execution_result;
        }

        SCP_DEBUG_CONTEXT(
            kHttpClient, http_context,
            absl::StrFormat(
                "Executing request on connection %p. Retry count: %lld",
                http_connection.get(), http_context.retry_count));

        return http_connection->Execute(http_context);
      });

  return SuccessExecutionResult();
}

void HttpClient::IncrementClientConnectionCreationError(
    const AsyncContext<HttpRequest, HttpResponse>& http_context) {
  if (!metric_router_) {
    return;
  }

  absl::flat_hash_map<absl::string_view, std::string> labels = {
      {kPbsClaimedIdentityLabel,
       GetClaimedIdentityOrUnknownValue(http_context)},
  };

  if (std::string* auth_domain =
          http_context.request->auth_context.authorized_domain.get();
      auth_domain != nullptr) {
    labels.try_emplace(kPbsAuthDomainLabel, *auth_domain);
  }

  opentelemetry::context::Context context;
  client_connection_creation_error_counter_->Add(1, labels);
}
}  // namespace privacy_sandbox::pbs_common
