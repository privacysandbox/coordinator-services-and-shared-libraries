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

#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/blob_storage_provider_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/interface/initializable_interface.h"
#include "cc/core/interface/nosql_database_provider_interface.h"
#include "cc/core/interface/token_provider_cache_interface.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cc/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/pbs_client_interface.h"
#include "cc/public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {

/// @brief Callbacks originating from the providers should have a higher
/// priority than the regular tasks because they are time-sensitive.
static constexpr core::AsyncPriority kDefaultAsyncPriorityForCallbackExecution =
    core::AsyncPriority::High;

/// @brief Blocking tasks are scheduled with a normal priority are can be
/// starved by a higher/urgent priority tasks.
static constexpr core::AsyncPriority
    kDefaultAsyncPriorityForBlockingIOTaskExecution =
        core::AsyncPriority::Normal;

/**
 * @brief Platform specific factory interface to provide platform specific
 * clients to PBS
 *
 */
class CloudPlatformDependencyFactoryInterface
    : public core::InitializableInterface {
 public:
  virtual ~CloudPlatformDependencyFactoryInterface() = default;

  /**
   * @brief Construct authorization token provider cache for PBS to use to talk
   * to remote PBS.
   *
   * @param async_executor
   * @param io_async_executor
   * @param http_client
   * @return std::unique_ptr<core::TokenProviderCacheInterface>
   */
  virtual std::unique_ptr<core::TokenProviderCacheInterface>
  ConstructAuthorizationTokenProviderCache(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      std::shared_ptr<core::HttpClientInterface> http_client) noexcept = 0;

  /**
   * @brief Construct a client for PBS to talk to authentication
   * endpoint
   *
   * @param async_executor
   * @param http_client
   * @return std::unique_ptr<core::AuthorizationProxyInterface>
   */
  virtual std::unique_ptr<core::AuthorizationProxyInterface>
  ConstructAuthorizationProxyClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::HttpClientInterface> http_client) noexcept = 0;

  // Constructs an AWS Client to talk to the authentication endpoint. This is
  // only used on GCP to authenticate requests that come from AWS PBS to GCP PBS
  // via DNS.
  virtual std::unique_ptr<core::AuthorizationProxyInterface>
  ConstructAwsAuthorizationProxyClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::HttpClientInterface> http_client) noexcept = 0;

  /**
   * @brief Construct a Blob Storage client for PBS to use AWS S3/GCP
   * CloudStorage/Azure Storage, etc.
   *
   * @param async_executor
   * @param io_async_executor
   * @return std::unique_ptr<core::BlobStorageProviderInterface>
   */
  virtual std::unique_ptr<core::BlobStorageProviderInterface>
  ConstructBlobStorageClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      core::AsyncPriority async_execution_priority =
          kDefaultAsyncPriorityForCallbackExecution,
      core::AsyncPriority io_async_execution_priority =
          kDefaultAsyncPriorityForBlockingIOTaskExecution) noexcept = 0;

  /**
   * @brief Constrict a NoSQL database client for PBS to use AWS Dynamo/GCP
   * Spanner/Azure CosmosDB, etc.
   *
   * @param async_executor
   * @param io_async_executor
   * @return std::unique_ptr<core::NoSQLDatabaseProviderInterface>
   */
  virtual std::unique_ptr<core::NoSQLDatabaseProviderInterface>
  ConstructNoSQLDatabaseClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      core::AsyncPriority async_execution_priority =
          kDefaultAsyncPriorityForCallbackExecution,
      core::AsyncPriority io_async_execution_priority =
          kDefaultAsyncPriorityForBlockingIOTaskExecution) noexcept = 0;

  virtual std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
  ConstructBudgetConsumptionHelper(
      google::scp::core::AsyncExecutorInterface* async_executor,
      google::scp::core::AsyncExecutorInterface*
          io_async_executor) noexcept = 0;
  /**
   * @brief Create a Instance Authorizer object for Instance Metadata client.
   *
   * @param http1_client
   * @return std::unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
   */
  virtual std::unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
  ConstructInstanceAuthorizer(
      std::shared_ptr<core::HttpClientInterface> http1_client) noexcept = 0;

  /**
   * @brief Construct Instance Metadata client
   *
   * @param http_client
   * @return std::unique_ptr<
   * cpio::client_providers::InstanceClientProviderInterface>
   */
  virtual std::unique_ptr<
      cpio::client_providers::InstanceClientProviderInterface>
  ConstructInstanceMetadataClient(
      std::shared_ptr<core::HttpClientInterface> http1_client,
      std::shared_ptr<core::HttpClientInterface> http2_client,
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
          auth_token_provider) noexcept = 0;

  /**
   * @brief Construct Metric Client
   *
   * @param async_executor
   * @param instance_client_provider
   * @return
   * std::unique_ptr<cpio::MetricClientInterface>
   */
  virtual std::unique_ptr<cpio::MetricClientInterface> ConstructMetricClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
      std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
          instance_client_provider) noexcept = 0;

  /**
   * @brief Construct Metric Router for Otel metrics collection
   * @param instance_client_provider
   * @return std::unique_ptr<core::MetricRouter>
   */
  virtual std::unique_ptr<core::MetricRouter> ConstructMetricRouter(
      std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
          instance_client_provider) noexcept = 0;

  /**
   * @brief Construct PBS client to talk to remote coordinator
   *
   * @param credential_provider
   * @param async_executor
   * @param http_client
   * @return std::unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
   */
  virtual std::unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
  ConstructRemoteCoordinatorPBSClient(
      std::shared_ptr<core::HttpClientInterface> http_client,
      std::shared_ptr<core::TokenProviderCacheInterface>
          auth_token_provider_cache) noexcept = 0;
};

}  // namespace google::scp::pbs
