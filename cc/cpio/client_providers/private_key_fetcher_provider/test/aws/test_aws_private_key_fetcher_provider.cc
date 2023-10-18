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

#include "test_aws_private_key_fetcher_provider.h"

#include <utility>

#include "core/interface/http_client_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"

using google::scp::core::ExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpRequest;
using google::scp::core::SuccessExecutionResult;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
// In integration test, localstack API_GATEWAY doesn't support signed request.
ExecutionResult TestAwsPrivateKeyFetcherProvider::SignHttpRequestUsingV4Signer(
    shared_ptr<HttpRequest>& http_request, const string& access_key,
    const string& secret_key, const string& security_token,
    const string& region) noexcept {
  return SuccessExecutionResult();
}

std::shared_ptr<PrivateKeyFetcherProviderInterface>
PrivateKeyFetcherProviderFactory::Create(
    const shared_ptr<HttpClientInterface>& http_client,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<AuthTokenProviderInterface>& auth_token_provider) {
  return make_shared<TestAwsPrivateKeyFetcherProvider>(
      http_client, role_credentials_provider);
}
}  // namespace google::scp::cpio::client_providers
