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
#include "core/authorization_proxy/src/authorization_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "core/authorization_proxy/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/http2_client.h"

using boost::system::error_code;
using google::scp::core::common::AutoExpiryConcurrentMap;
using google::scp::core::common::kZeroUuid;
using nghttp2::asio_http2::host_service_from_uri;
using std::function;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

static constexpr const char kAuthorizationProxy[] = "AuthorizationProxy";

static constexpr int kAuthorizationCacheEntryLifetimeSeconds = 150;

namespace google::scp::core {

void OnBeforeGarbageCollection(std::string&,
                               shared_ptr<AuthorizationProxy::CacheEntry>&,
                               function<void(bool)> should_delete_entry) {
  should_delete_entry(true);
}

ExecutionResult AuthorizationProxy::Init() noexcept {
  error_code http2_error_code;
  string scheme;
  string service;
  if (host_service_from_uri(http2_error_code, scheme, host_, service,
                            *server_endpoint_uri_)) {
    auto execution_result =
        FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_INVALID_CONFIG);
    SCP_ERROR(kAuthorizationProxy, kZeroUuid, execution_result,
              "Failed to parse URI with boost error_code: %s",
              http2_error_code.message().c_str());
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
    const string& server_endpoint_url,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<HttpClientInterface>& http_client,
    std::unique_ptr<HttpRequestResponseAuthInterceptorInterface> http_helper)
    : cache_(kAuthorizationCacheEntryLifetimeSeconds,
             false /* extend_entry_lifetime_on_access */,
             false /* block_entry_while_eviction */,
             bind(&OnBeforeGarbageCollection, _1, _2, _3), async_executor),
      server_endpoint_uri_(make_shared<string>(server_endpoint_url)),
      http_client_(http_client),
      http_helper_(move(http_helper)) {}

ExecutionResult AuthorizationProxy::Authorize(
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>&
        authorization_context) noexcept {
  if (!authorization_context.request) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_BAD_REQUEST);
  }

  const auto& request = *authorization_context.request;
  if (!request.authorization_metadata.IsValid()) {
    return FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_BAD_REQUEST);
  }

  // Q: Is decoded token necessary here?
  shared_ptr<CacheEntry> cache_entry_result;
  // TODO Current map doesn't allow custom types i.e. AuthorizationMetadata
  // to be part of key because there is a need to specialise std::hash template
  // for AuthorizationMetadata, keeping string for now as the key.
  auto key_value_pair = make_pair(request.authorization_metadata.GetKey(),
                                  make_shared<CacheEntry>());
  auto execution_result = cache_.Insert(key_value_pair, cache_entry_result);
  if (!execution_result.Successful()) {
    if (execution_result.status_code ==
        errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED) {
      return RetryExecutionResult(execution_result.status_code);
    }

    if (execution_result.status_code !=
        errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS) {
      return execution_result;
    }

    if (cache_entry_result->is_loaded) {
      authorization_context.response =
          make_shared<AuthorizationProxyResponse>();
      authorization_context.response->authorized_metadata =
          cache_entry_result->authorized_metadata;
      authorization_context.result = SuccessExecutionResult();
      authorization_context.Finish();
      return SuccessExecutionResult();
    }

    return RetryExecutionResult(
        errors::SC_AUTHORIZATION_PROXY_AUTH_REQUEST_INPROGRESS);
  }

  // Cache entry was not present, inserted.
  execution_result = cache_.DisableEviction(key_value_pair.first);
  if (!execution_result.Successful()) {
    cache_.Erase(key_value_pair.first);
    return RetryExecutionResult(
        errors::SC_AUTHORIZATION_PROXY_AUTH_REQUEST_INPROGRESS);
  }

  auto http_request = make_shared<HttpRequest>();
  http_request->method = HttpMethod::POST;
  http_request->path = server_endpoint_uri_;
  http_request->headers = make_shared<HttpHeaders>();

  execution_result = http_helper_->PrepareRequest(
      request.authorization_metadata, *http_request);
  if (!execution_result.Successful()) {
    SCP_ERROR(kAuthorizationProxy, kZeroUuid, execution_result,
              "Failed adding headers to request");
    cache_.Erase(key_value_pair.first);
    return FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_BAD_REQUEST);
  }

  AsyncContext<HttpRequest, HttpResponse> http_context(
      move(http_request),
      bind(&AuthorizationProxy::HandleAuthorizeResponse, this,
           authorization_context, key_value_pair.first, _1),
      authorization_context);
  auto result = http_client_->PerformRequest(http_context);
  if (!result.Successful()) {
    cache_.Erase(key_value_pair.first);
    return RetryExecutionResult(
        errors::SC_AUTHORIZATION_PROXY_REMOTE_UNAVAILABLE);
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

  authorization_context.response = make_shared<AuthorizationProxyResponse>();
  authorization_context.response->authorized_metadata = std::move(*metadata_or);

  // Update cache entry
  shared_ptr<CacheEntry> cache_entry;
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
}  // namespace google::scp::core
