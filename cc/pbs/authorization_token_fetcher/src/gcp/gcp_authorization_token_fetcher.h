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

#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/token_fetcher_interface.h"

namespace google::scp::pbs {
class GcpAuthorizationTokenFetcher : public core::TokenFetcherInterface {
 public:
  /**
   * @brief Construct a new Gcp Authorization Token Fetcher object
   *
   * @param http_client Client to contact the metadata server (must be HTTP1).
   * @param token_target_audience_uri The target audience (URI) the acquired
   * token will be used for.
   * @param async_executor The executor to asynchronously fetch the token on.
   */
  GcpAuthorizationTokenFetcher(
      std::shared_ptr<core::HttpClientInterface> http_client,
      const std::string& token_target_audience_uri,
      std::shared_ptr<core::AsyncExecutorInterface> async_executor);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult FetchToken(
      core::AsyncContext<core::FetchTokenRequest, core::FetchTokenResponse>
          fetch_token_context) noexcept override;

 private:
  // Method for handling the http_context after the Http Client finishes
  // processing.
  void ProcessHttpResponse(
      core::AsyncContext<core::FetchTokenRequest, core::FetchTokenResponse>
          fetch_token_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse> http_context);

  std::shared_ptr<core::HttpClientInterface> http_client_;
  std::string token_target_audience_uri_;
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  std::string host_url_;
};

}  // namespace google::scp::pbs
