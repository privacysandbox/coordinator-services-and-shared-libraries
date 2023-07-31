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
#include <utility>

#include "core/interface/credentials_provider_interface.h"
#include "core/interface/token_fetcher_interface.h"

namespace google::scp::pbs {
class AwsAuthorizationTokenFetcher : public core::TokenFetcherInterface {
 public:
  AwsAuthorizationTokenFetcher(
      std::string gateway_endpoint, std::string region,
      std::shared_ptr<core::CredentialsProviderInterface> credentials_provider)
      : gateway_endpoint_(std::move(gateway_endpoint)),
        region_(std::move(region)),
        credentials_provider_(credentials_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult FetchToken(
      core::AsyncContext<core::FetchTokenRequest,
                         core::FetchTokenResponse>) noexcept override;

 protected:
  void OnGetCredentialsCallback(
      core::AsyncContext<core::FetchTokenRequest, core::FetchTokenResponse>
          token_request_context,
      core::AsyncContext<core::GetCredentialsRequest,
                         core::GetCredentialsResponse>&) noexcept;

  const std::string gateway_endpoint_;
  const std::string region_;
  std::shared_ptr<core::CredentialsProviderInterface> credentials_provider_;
};
}  // namespace google::scp::pbs
