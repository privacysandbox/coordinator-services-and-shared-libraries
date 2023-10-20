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

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "core/authorization_service/src/aws_authorizer.h"
#include "core/common/auto_expiry_concurrent_map/mock/mock_auto_expiry_concurrent_map.h"

namespace google::scp::core::authorization_service::mock {

class MockAuthorizationServiceWithOverrides : public AwsAuthorizer {
 public:
  MockAuthorizationServiceWithOverrides(
      const std::string& server_endpoint, const std::string& aws_region,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<HttpClientInterface>& http_client)
      : AwsAuthorizer(server_endpoint, aws_region, async_executor,
                      http_client) {
    authorization_tokens_ = std::make_unique<
        common::auto_expiry_concurrent_map::mock::MockAutoExpiryConcurrentMap<
            std::string, std::shared_ptr<AwsAuthorizationTokenCacheEntry>>>(
        100, false, false,
        std::bind(
            &MockAuthorizationServiceWithOverrides::OnBeforeGarbageCollection,
            this, std::placeholders::_1, std::placeholders::_3),
        async_executor);
  }

  void OnBeforeGarbageCollection(
      std::string& cache_entry_key,
      std::function<void(bool)> should_delete_entry) noexcept {
    std::shared_ptr<AwsAuthorizationTokenCacheEntry> cached_auth_token_entry;
    AwsAuthorizer::OnBeforeGarbageCollection(
        cache_entry_key, cached_auth_token_entry, should_delete_entry);
  }

  void HandleHttpResponse(
      AsyncContext<AuthorizationRequest, AuthorizationResponse>&
          authorization_context,
      std::string& token,
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    AwsAuthorizer::HandleHttpResponse(authorization_context, token,
                                      http_context);
  }

  auto* GetAuthorizationTokensMap() {
    return static_cast<
        common::auto_expiry_concurrent_map::mock::MockAutoExpiryConcurrentMap<
            std::string, std::shared_ptr<AwsAuthorizationTokenCacheEntry>>*>(
        authorization_tokens_.get());
  }
};
}  // namespace google::scp::core::authorization_service::mock
