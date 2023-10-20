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

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/http_client_interface.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "http1_curl_wrapper.h"

namespace google::scp::core {

/*! @copydoc HttpClientInterface
 *  This client is explicitly an HTTP1 client, not HTTP2.
 */
class Http1CurlClient : public HttpClientInterface {
 public:
  /**
   * @brief Construct a new CURL Client object
   *
   * @param async_executor an instance of the async executor.
   * @param retry_strategy_type retry strategy type.
   * @param time_duraton_ms delay time duration in ms for http client retry
   * strategy.
   * @param total_retries total retry counts.
   */
  explicit Http1CurlClient(
      const std::shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<AsyncExecutorInterface>& io_async_executor,
      std::shared_ptr<Http1CurlWrapperProvider> curl_wrapper_provider =
          std::make_shared<Http1CurlWrapperProvider>(),
      common::RetryStrategyOptions retry_strategy_options =
          common::RetryStrategyOptions(common::RetryStrategyType::Exponential,
                                       kDefaultRetryStrategyDelayInMs,
                                       kDefaultRetryStrategyMaxRetries));

  ExecutionResult Init() noexcept override;
  ExecutionResult Run() noexcept override;
  ExecutionResult Stop() noexcept override;

  ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept override;

 private:
  std::shared_ptr<Http1CurlWrapperProvider> curl_wrapper_provider_;

  const std::shared_ptr<AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;
  /// Operation dispatcher
  common::OperationDispatcher operation_dispatcher_;
};

}  // namespace google::scp::core
