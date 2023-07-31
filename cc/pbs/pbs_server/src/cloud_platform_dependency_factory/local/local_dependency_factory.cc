// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "local_dependency_factory.h"

#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/pbs_client/src/pbs_client.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::CredentialsProviderInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::MetricClientOptions;

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

namespace google::scp::pbs {

static constexpr char kLocalDependencyProvider[] = "kLocalDependencyProvider";

ExecutionResult LocalDependencyFactory::Init() noexcept {
  INFO(kLocalDependencyProvider, kZeroUuid, kZeroUuid,
       "Initializing Local dependency factory");

  RETURN_IF_FAILURE(ReadConfigurations());
  return SuccessExecutionResult();
}

ExecutionResult LocalDependencyFactory::ReadConfigurations() {
  return SuccessExecutionResult();
}

std::unique_ptr<core::TokenProviderCacheInterface>
LocalDependencyFactory::ConstructAuthorizationTokenProviderCache(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  // TODO
  return nullptr;
}

unique_ptr<AuthorizationProxyInterface>
LocalDependencyFactory::ConstructAuthorizationProxyClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  // TODO
  return nullptr;
}

unique_ptr<BlobStorageProviderInterface>
LocalDependencyFactory::ConstructBlobStorageClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor) noexcept {
  // TODO
  return nullptr;
}

unique_ptr<NoSQLDatabaseProviderInterface>
LocalDependencyFactory::ConstructNoSQLDatabaseClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor) noexcept {
  // TODO
  return nullptr;
}

unique_ptr<cpio::client_providers::MetricClientProviderInterface>
LocalDependencyFactory::ConstructMetricClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  // TODO
  return nullptr;
}

unique_ptr<cpio::client_providers::InstanceClientProviderInterface>
LocalDependencyFactory::ConstructInstanceMetadataClient(
    std::shared_ptr<core::HttpClientInterface> http1_client,
    std::shared_ptr<core::HttpClientInterface> http2_client,
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
        auth_token_provider) noexcept {
  // TODO
  return nullptr;
}

unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
LocalDependencyFactory::ConstructInstanceAuthorizer(
    std::shared_ptr<core::HttpClientInterface> http1_client) noexcept {
  // TODO
  return nullptr;
}

unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
LocalDependencyFactory::ConstructRemoteCoordinatorPBSClient(
    std::shared_ptr<core::HttpClientInterface> http_client,
    std::shared_ptr<core::TokenProviderCacheInterface>
        auth_token_provider_cache) noexcept {
  // TODO
  return nullptr;
}

}  // namespace google::scp::pbs
