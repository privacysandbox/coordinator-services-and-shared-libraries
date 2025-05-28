/*
 * Copyright 2025 Google LLC
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

#ifndef CC_CORE_HTTP2_CLIENT_SRC_SYNCHRONOUS_HTTP2_CLIENT_H_
#define CC_CORE_HTTP2_CLIENT_SRC_SYNCHRONOUS_HTTP2_CLIENT_H_

#include <memory>

#include "cc/core/common/operation_dispatcher/src/retry_strategy.h"
#include "cc/core/http2_client/src/http_connection_pool.h"
#include "cc/core/http2_client/src/http_options.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs_common {

struct SyncHttpClientResponse {
  ExecutionResult execution_result;
  std::unique_ptr<HttpResponse> http_response;
};

class SyncHttpClient {
 public:
  explicit SyncHttpClient(HttpClientOptions options = HttpClientOptions());

  ~SyncHttpClient();

  SyncHttpClientResponse PerformRequest(
      const HttpRequest& http_request) noexcept;

 private:
  ExecutionResult TryRequest(
      AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept;
  ExecutionResultOr<TimeDuration> CheckForRetries(
      AsyncContext<HttpRequest, HttpResponse>& http_context);

  std::unique_ptr<HttpConnectionPool> http_connection_pool_;
  RetryStrategy retry_strategy_;
};

}  // namespace privacy_sandbox::pbs_common

#endif  // CC_CORE_HTTP2_CLIENT_SRC_SYNCHRONOUS_HTTP2_CLIENT_H_
