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

#include "private_key_fetching_client_provider.h"

#include <functional>
#include <memory>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/private_key_fetching_client_provider_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "private_key_fetching_client_utils.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::cpio::client_providers::PrivateKeyFetchingRequest;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

static constexpr char kPrivateKeyFetchingClientProvider[] =
    "PrivateKeyFetchingClientProvider";

namespace google::scp::cpio::client_providers {
ExecutionResult PrivateKeyFetchingClientProvider::Init() noexcept {
  if (!http_client_) {
    auto execution_result = FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_HTTP_CLIENT_NOT_FOUND);
    ERROR(kPrivateKeyFetchingClientProvider, kZeroUuid, kZeroUuid,
          execution_result, "Failed to get http client.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetchingClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetchingClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetchingClientProvider::FetchPrivateKey(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context) noexcept {
  auto base_uri =
      *private_key_fetching_context.request->private_key_service_base_uri;
  auto key_uri = *private_key_fetching_context.request->key_id;
  auto uri = make_shared<Uri>(base_uri + "/" + key_uri);
  auto http_request = make_shared<HttpRequest>();
  http_request->method = HttpMethod::GET;
  http_request->path = move(uri);

  AsyncContext<HttpRequest, HttpRequest> sign_http_request_context(
      move(http_request),
      bind(&PrivateKeyFetchingClientProvider::SignHttpRequestCallback, this,
           private_key_fetching_context, _1));

  return SignHttpRequest(
      sign_http_request_context,
      private_key_fetching_context.request->service_region,
      private_key_fetching_context.request->account_identity);
}

void PrivateKeyFetchingClientProvider::SignHttpRequestCallback(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context,
    AsyncContext<HttpRequest, HttpRequest>&
        sign_http_request_context) noexcept {
  auto execution_result = sign_http_request_context.result;
  if (!execution_result.Successful()) {
    ERROR(kPrivateKeyFetchingClientProvider, kZeroUuid, kZeroUuid,
          execution_result, "Failed to sign http request.");
    private_key_fetching_context.result = execution_result;
    private_key_fetching_context.Finish();
    return;
  }

  AsyncContext<HttpRequest, HttpResponse> http_client_context(
      sign_http_request_context.response,
      bind(&PrivateKeyFetchingClientProvider::PrivateKeyFetchingCallback, this,
           private_key_fetching_context, _1));
  execution_result = http_client_->PerformRequest(http_client_context);
  if (!execution_result.Successful()) {
    ERROR(kPrivateKeyFetchingClientProvider, kZeroUuid, kZeroUuid,
          execution_result, "Failed to perform sign http request.");
    private_key_fetching_context.result = execution_result;
    private_key_fetching_context.Finish();
  }
}

void PrivateKeyFetchingClientProvider::PrivateKeyFetchingCallback(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  private_key_fetching_context.result = http_client_context.result;
  if (!http_client_context.result.Successful()) {
    ERROR_CONTEXT(
        kPrivateKeyFetchingClientProvider, private_key_fetching_context,
        private_key_fetching_context.result, "Failed to fetch private key.");
    private_key_fetching_context.Finish();
    return;
  }

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      http_client_context.response->body, response);
  if (!result.Successful()) {
    ERROR_CONTEXT(
        kPrivateKeyFetchingClientProvider, private_key_fetching_context,
        private_key_fetching_context.result, "Failed to parse private key.");
    private_key_fetching_context.result = result;
    private_key_fetching_context.Finish();
    return;
  }

  private_key_fetching_context.response =
      make_shared<PrivateKeyFetchingResponse>(response);
  private_key_fetching_context.Finish();
}

}  // namespace google::scp::cpio::client_providers
