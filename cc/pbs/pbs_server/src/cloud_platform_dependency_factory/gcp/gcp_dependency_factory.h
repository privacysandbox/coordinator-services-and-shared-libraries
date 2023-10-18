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

#include "pbs/interface/cloud_platform_dependency_factory_interface.h"

namespace google::scp::pbs {
class GcpDependencyFactory : public CloudPlatformDependencyFactoryInterface {
 public:
  GcpDependencyFactory(
      std::shared_ptr<core::ConfigProviderInterface> config_provider);

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

 protected:
  core::ExecutionResult ReadConfigurations();

  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  // Configurations here
  std::string budget_key_table_name_;
  std::string partition_lock_table_name_;
  std::string auth_service_endpoint_;

  // Reporting origin information of this coordinator for the other remote
  // coordinator.
  std::string reporting_origin_for_remote_coordinator_;
  std::string remote_coordinator_endpoint_;
  std::string remote_coordinator_auth_gateway_endpoint_;

  std::string metrics_namespace_;
  bool metrics_batch_push_enabled_;
  core::TimeDuration metrics_batch_time_duration_ms_;
};

}  // namespace google::scp::pbs
