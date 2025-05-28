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

#ifndef CC_CORE_HTTP2_CLIENT_SRC_HTTP_OPTIONS_H_
#define CC_CORE_HTTP2_CLIENT_SRC_HTTP_OPTIONS_H_

#include "cc/core/common/operation_dispatcher/src/retry_strategy.h"

namespace privacy_sandbox::pbs_common {

struct HttpClientOptions {
  HttpClientOptions()
      : retry_strategy_options(RetryStrategyOptions(
            RetryStrategyType::Exponential, kDefaultRetryStrategyDelayInMs,
            kDefaultRetryStrategyMaxRetries)),
        max_connections_per_host(kDefaultMaxConnectionsPerHost),
        http2_read_timeout_in_sec(kDefaultHttp2ReadTimeoutInSeconds) {}

  HttpClientOptions(RetryStrategyOptions retry_strategy_options,
                    size_t max_connections_per_host,
                    TimeDuration http2_read_timeout_in_sec)
      : retry_strategy_options(retry_strategy_options),
        max_connections_per_host(max_connections_per_host),
        http2_read_timeout_in_sec(http2_read_timeout_in_sec) {}

  /// Retry strategy options.
  const RetryStrategyOptions retry_strategy_options;
  /// Max http connections per host.
  const size_t max_connections_per_host;
  /// nghttp client read timeout.
  const TimeDuration http2_read_timeout_in_sec;
};

}  // namespace privacy_sandbox::pbs_common

#endif  // CC_CORE_HTTP2_CLIENT_SRC_HTTP_OPTIONS_H_
