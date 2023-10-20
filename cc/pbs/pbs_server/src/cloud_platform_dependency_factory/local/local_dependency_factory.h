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
#include <vector>

#include "core/interface/partition_types.h"
#include "pbs/interface/cloud_platform_dependency_factory_interface.h"

namespace google::scp::pbs {

static constexpr core::PartitionId kGlobalPartitionId = {0, 0};

class LocalDependencyFactory : public CloudPlatformDependencyFactoryInterface {
 public:
  LocalDependencyFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      std::vector<core::PartitionId> partition_ids = {kGlobalPartitionId})
      : config_provider_(config_provider),
        partition_ids_(std::move(partition_ids)) {}

  core::ExecutionResult Init() noexcept override;

  std::unique_ptr<core::TokenProviderCacheInterface>
  ConstructAuthorizationTokenProviderCache(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      std::shared_ptr<core::HttpClientInterface> http_client) noexcept override;

  std::unique_ptr<core::AuthorizationProxyInterface>
  ConstructAuthorizationProxyClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::HttpClientInterface> http_client) noexcept override;

  std::unique_ptr<core::BlobStorageProviderInterface>
  ConstructBlobStorageClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      core::AsyncPriority async_execution_priority =
          kDefaultAsyncPriorityForCallbackExecution,
      core::AsyncPriority io_async_execution_priority =
          kDefaultAsyncPriorityForBlockingIOTaskExecution) noexcept override;

  std::unique_ptr<core::NoSQLDatabaseProviderInterface>
  ConstructNoSQLDatabaseClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      core::AsyncPriority async_execution_priority =
          kDefaultAsyncPriorityForCallbackExecution,
      core::AsyncPriority io_async_execution_priority =
          kDefaultAsyncPriorityForBlockingIOTaskExecution) noexcept override;

  std::unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
  ConstructInstanceAuthorizer(std::shared_ptr<core::HttpClientInterface>
                                  http1_client) noexcept override;

  std::unique_ptr<cpio::client_providers::InstanceClientProviderInterface>
  ConstructInstanceMetadataClient(
      std::shared_ptr<core::HttpClientInterface> http1_client,
      std::shared_ptr<core::HttpClientInterface> http2_client,
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
          auth_token_provider) noexcept override;

  std::unique_ptr<cpio::MetricClientInterface> ConstructMetricClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
          instance_client_provider) noexcept override;

  std::unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
  ConstructRemoteCoordinatorPBSClient(
      std::shared_ptr<core::HttpClientInterface> http_client,
      std::shared_ptr<core::TokenProviderCacheInterface>
          auth_token_provider_cache) noexcept override;

 private:
  core::ExecutionResult ReadConfigurations();

  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  std::string remote_coordinator_endpoint_ = "http://othercoordinator.com:8080";
  std::string reporting_origin_ = "reporting_origin";
  std::vector<core::PartitionId> partition_ids_;
};

}  // namespace google::scp::pbs
