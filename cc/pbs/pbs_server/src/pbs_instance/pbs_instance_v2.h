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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/interface/async_executor_interface.h"
#include "core/interface/authorization_proxy_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_request_route_resolver_interface.h"
#include "core/interface/http_request_router_interface.h"
#include "core/interface/http_server_interface.h"
#include "core/interface/lease_manager_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/partition_namespace_interface.h"
#include "core/interface/partition_types.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/service_interface.h"
#include "core/interface/token_provider_cache_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/front_end_service/src/front_end_service.h"
#include "pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/interface/pbs_partition_manager_interface.h"
#include "pbs/partition/src/pbs_partition.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {
/**
 * @brief PBSInstanceV2, similar to 'PBSInstance' in functionality but
 * implements Partitioning concepts but still runs a single global partition
 * similar to PBSInstance.
 *
 */
class PBSInstanceV2 : public core::ServiceInterface {
 public:
  explicit PBSInstanceV2(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<CloudPlatformDependencyFactoryInterface>&
          platform_dependency_factory)
      : config_provider_(config_provider),
        platform_dependency_factory_(platform_dependency_factory) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

 protected:
  /**
   * @brief Construct dependencies of PBSInstance
   *
   * @return core::ExecutionResult
   */
  core::ExecutionResult ConstructDependencies() noexcept;

  /**
   * @brief Lease transition callback for Global Partition's lease changes.
   * This will be invoked by Lease Manager upon any change to the lease on
   * global partition.
   */
  void PartitionLeaseTransitionCallback(
      core::LeaseTransitionType lease_transition_type,
      std::optional<core::LeaseInfo> lease_info);

  /**
   * @brief Lease transition handler for ACQUIRED.
   */
  void PartitionLeaseAcquired();

  /**
   * @brief Lease transition handler for NOT_ACQUIRED.
   * @param lease_owner_info the current lease owner's information.
   */
  void PartitionLeaseUnableToAcquire(
      std::optional<core::LeaseInfo> lease_owner_info);

  /**
   * @brief Lease transition handler for LOST.
   */
  void PartitionLeaseLost();

  /**
   * @brief Lease transition handler for RENEWED.
   */
  void PartitionLeaseRenewed();

  /**
   * @brief Get the current instance's identifier and exposed IPv4
   * address.
   *
   * Returns an empty string for each of the ID or IPv4 if the corresponding
   * value is not available.
   */
  std::pair<std::string, std::string> GetInstanceIDAndIPv4Address();

  // Cloud Platform Dependency provider factory
  std::shared_ptr<CloudPlatformDependencyFactoryInterface>
      platform_dependency_factory_;

  // Config
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  PBSInstanceConfig pbs_instance_config_;

  // Partition
  std::shared_ptr<PBSPartitionManagerInterface> partition_manager_;
  std::shared_ptr<core::PartitionNamespaceInterface> partition_namespace_;
  std::shared_ptr<core::HttpRequestRouterInterface> request_router_;
  std::shared_ptr<core::HttpRequestRouteResolverInterface>
      request_route_resolver_;
  PBSPartition::Dependencies partition_dependencies_;
  std::vector<core::PartitionId> partitions_set_;

  // Executors
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;

  // Misc. Clients
  std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
      instance_client_provider_;
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  // Lease Manager and Leasable Lock
  std::shared_ptr<core::AsyncExecutorInterface>
      async_executor_for_leasable_lock_nosql_database_;
  std::shared_ptr<core::AsyncExecutorInterface>
      io_async_executor_for_leasable_lock_nosql_database_;
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_leasable_lock_;
  std::shared_ptr<core::LeaseManagerInterface> lease_manager_service_;
  core::LeaseInfo lease_acquirer_info_;

  // Auth N/Z
  std::shared_ptr<core::AuthorizationProxyInterface> authorization_proxy_;
  std::shared_ptr<core::AuthorizationProxyInterface>
      pass_thru_authorization_proxy_;
  std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
      auth_token_provider_;
  std::shared_ptr<core::TokenProviderCacheInterface> auth_token_provider_cache_;

  // Store
  std::shared_ptr<core::BlobStorageProviderInterface>
      blob_storage_provider_for_journal_service_;
  std::shared_ptr<core::BlobStorageProviderInterface>
      blob_storage_provider_for_checkpoint_service_;
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_;

  // HTTP
  std::shared_ptr<core::HttpServerInterface> http_server_;
  std::shared_ptr<core::HttpServerInterface> health_http_server_;
  std::shared_ptr<core::HttpClientInterface> http1_client_;
  std::shared_ptr<core::HttpClientInterface> http2_client_;
  std::shared_ptr<core::ServiceInterface> health_service_;
  std::shared_ptr<core::RemoteTransactionManagerInterface>
      remote_transaction_manager_;
  std::shared_ptr<FrontEndService> front_end_service_;
  std::shared_ptr<pbs::PrivacyBudgetServiceClientInterface>
      remote_coordinator_pbs_client_;

  // Represents Load task that is scheduled.
  std::function<bool()> partition_load_cancellation_callback_;
};
}  // namespace google::scp::pbs
