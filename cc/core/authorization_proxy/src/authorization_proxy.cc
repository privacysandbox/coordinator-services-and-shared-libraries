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
#include "cc/core/authorization_proxy/src/authorization_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include <nghttp2/asio_http2_client.h>

#include "cc/core/authorization_proxy/src/error_codes.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/http_types.h"

static constexpr const char kAuthorizationProxy[] = "AuthorizationProxy";

static constexpr int kAuthorizationCacheEntryLifetimeSeconds = 150;

namespace privacy_sandbox::pbs_common {

using boost::system::error_code;
using nghttp2::asio_http2::host_service_from_uri;

void OnBeforeGarbageCollection(std::string&,
                               std::shared_ptr<AuthorizationProxy::CacheEntry>&,
                               std::function<void(bool)> should_delete_entry) {
  should_delete_entry(true);
}

ExecutionResult AuthorizationProxy::Init() noexcept {
  error_code http2_error_code;
  std::string scheme;
  std::string service;
  if (host_service_from_uri(http2_error_code, scheme, host_, service,
                            *server_endpoint_uri_)) {
    auto execution_result =
        FailureExecutionResult(SC_AUTHORIZATION_PROXY_INVALID_CONFIG);
    SCP_ERROR(kAuthorizationProxy, kZeroUuid, execution_result,
              absl::StrFormat("Failed to parse URI with boost error_code: %s",
                              http2_error_code.message().c_str()));
    return execution_result;
  }
  return cache_.Init();
}

ExecutionResult AuthorizationProxy::Run() noexcept {
  return cache_.Run();
}

ExecutionResult AuthorizationProxy::Stop() noexcept {
  return cache_.Stop();
}

AuthorizationProxy::AuthorizationProxy(
    const std::string& server_endpoint_url,
    const std::shared_ptr<AsyncExecutorInterface>& async_executor,
    const std::shared_ptr<HttpClientInterface>& http_client,
    std::unique_ptr<HttpRequestResponseAuthInterceptorInterface> http_helper)
    : cache_(kAuthorizationCacheEntryLifetimeSeconds,
             false /* extend_entry_lifetime_on_access */,
             false /* block_entry_while_eviction */,
             bind(&OnBeforeGarbageCollection, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3),
             async_executor),
      server_endpoint_uri_(make_shared<std::string>(server_endpoint_url)),
      http_client_(http_client),
      http_helper_(std::move(http_helper)) {}

ExecutionResult AuthorizationProxy::Authorize(
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
        authorization_context) noexcept {
  if (!authorization_context.request) {
    return FailureExecutionResult(SC_AUTHORIZATION_PROXY_BAD_REQUEST);
  }

  const auto& request = *authorization_context.request;
  if (!request.authorization_metadata.IsValid()) {
    return FailureExecutionResult(SC_AUTHORIZATION_PROXY_BAD_REQUEST);
  }

  // Q: Is decoded token necessary here?
  std::shared_ptr<CacheEntry> cache_entry_result;
  // TODO Current map doesn't allow custom types i.e. AuthorizationMetadata
  // to be part of key because there is a need to specialise std::hash template
  // for AuthorizationMetadata, keeping string for now as the key.
  auto key_value_pair = make_pair(request.authorization_metadata.GetKey(),
                                  std::make_shared<CacheEntry>());
  auto execution_result = cache_.Insert(key_value_pair, cache_entry_result);
  if (!execution_result.Successful()) {
    if (execution_result.status_code ==
        SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED) {
      return RetryExecutionResult(execution_result.status_code);
    }

    if (execution_result.status_code !=
        SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS) {
      return execution_result;
    }

    if (cache_entry_result->is_loaded) {
      authorization_context.response =
          std::make_shared<AuthorizationProxyResponse>();
      authorization_context.response->authorized_metadata =
          cache_entry_result->authorized_metadata;
      authorization_context.result = SuccessExecutionResult();
      authorization_context.Finish();
      return SuccessExecutionResult();
    }

    return RetryExecutionResult(SC_AUTHORIZATION_PROXY_AUTH_REQUEST_INPROGRESS);
  }

  // Cache entry was not present, inserted.
  execution_result = cache_.DisableEviction(key_value_pair.first);
  if (!execution_result.Successful()) {
    cache_.Erase(key_value_pair.first);
    return RetryExecutionResult(SC_AUTHORIZATION_PROXY_AUTH_REQUEST_INPROGRESS);
  }

  auto http_request = std::make_shared<HttpRequest>();
  http_request->method = HttpMethod::POST;
  http_request->path = server_endpoint_uri_;
  http_request->headers = std::make_shared<HttpHeaders>();

  execution_result = http_helper_->PrepareRequest(
      request.authorization_metadata, *http_request);
  if (!execution_result.Successful()) {
    SCP_ERROR(kAuthorizationProxy, kZeroUuid, execution_result,
              "Failed adding headers to request");
    cache_.Erase(key_value_pair.first);
    return FailureExecutionResult(SC_AUTHORIZATION_PROXY_BAD_REQUEST);
  }

  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(http_request),
      bind(&AuthorizationProxy::HandleAuthorizeResponse, this,
           authorization_context, key_value_pair.first, std::placeholders::_1),
      authorization_context);
  auto result = http_client_->PerformRequest(http_context);
  if (!result.Successful()) {
    cache_.Erase(key_value_pair.first);
    return RetryExecutionResult(SC_AUTHORIZATION_PROXY_REMOTE_UNAVAILABLE);
  }

  return SuccessExecutionResult();
}

void AuthorizationProxy::HandleAuthorizeResponse(
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
        authorization_context,
    std::string& cache_entry_key,
    AsyncContext<HttpRequest, HttpResponse>& http_context) {
  if (!http_context.result.Successful()) {
    cache_.Erase(cache_entry_key);
    // Bubbling client error up the stack
    authorization_context.result = http_context.result;
    authorization_context.Finish();
    return;
  }

  auto metadata_or = http_helper_->ObtainAuthorizedMetadataFromResponse(
      authorization_context.request->authorization_metadata,
      *(http_context.response));
  if (!metadata_or.Successful()) {
    cache_.Erase(cache_entry_key);
    authorization_context.result = metadata_or.result();
    authorization_context.Finish();
    return;
  }

  authorization_context.response =
      std::make_shared<AuthorizationProxyResponse>();
  authorization_context.response->authorized_metadata = std::move(*metadata_or);

  // Update cache entry
  std::shared_ptr<CacheEntry> cache_entry;
  auto execution_result = cache_.Find(cache_entry_key, cache_entry);
  if (!execution_result.Successful()) {
    SCP_DEBUG_CONTEXT(kAuthorizationProxy, authorization_context,
                      "Cannot find the cached entry.");
    authorization_context.result = SuccessExecutionResult();
    authorization_context.Finish();
  }

  cache_entry->authorized_metadata =
      authorization_context.response->authorized_metadata;
  cache_entry->is_loaded = true;

  execution_result = cache_.EnableEviction(cache_entry_key);
  if (!execution_result.Successful()) {
    cache_.Erase(cache_entry_key);
  }

  authorization_context.result = SuccessExecutionResult();
  authorization_context.Finish();
}
}  // namespace privacy_sandbox::pbs_common
