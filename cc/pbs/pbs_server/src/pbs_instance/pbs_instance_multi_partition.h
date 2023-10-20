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

#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_forwarder/src/http2_forwarder.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/authorization_proxy_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/credentials_provider_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_server_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/lease_manager_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/service_interface.h"
#include "core/interface/token_provider_cache_interface.h"
#include "core/interface/traffic_forwarder_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/front_end_service/src/front_end_service.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/front_end_service_interface.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/interface/pbs_partition_interface.h"
#include "pbs/partition/src/pbs_partition.h"
#include "pbs/partition_lease_event_sink/src/partition_lease_event_sink.h"
#include "pbs/partition_lease_preference_applier/src/partition_lease_preference_applier.h"
#include "pbs/partition_manager/src/pbs_partition_manager.h"
#include "pbs/partition_namespace/src/pbs_partition_namespace.h"
#include "pbs/partition_request_router/src/http_request_route_resolver_for_partition.h"
#include "pbs/partition_request_router/src/transaction_request_router_for_partition.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"
#include "pbs/transactions/src/consume_budget_command_factory.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {
/**
 * @brief PBSInstanceMultiPartition implements partitioning concepts and runs
 * multiple partitions.
 *
 */
class PBSInstanceMultiPartition : public core::ServiceInterface {
 public:
  explicit PBSInstanceMultiPartition(
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
   * @brief Create a Leasable Lock for the given partition with ID
   *
   * @return std::shared_ptr<core::LeasableLockInterface>
   */
  std::shared_ptr<core::LeasableLockInterface> CreateLeasableLockForPartition(
      const core::PartitionId&, const core::LeaseInfo&);

  /**
   * @brief Create a Leasable Lock for the given virtual node with ID
   *
   * @return std::shared_ptr<core::LeasableLockInterface>
   */
  std::shared_ptr<core::LeasableLockInterface> CreateLeasableLockForVNode(
      const core::common::Uuid&, const core::LeaseInfo&);

  /**
   * @brief Get the current instance's identifier and exposed IPv4
   * address.
   *
   * Returns an empty string for each of the ID or IPv4 if the corresponding
   * value is not available.
   */
  std::pair<std::string, std::string> GetInstanceIDAndIPv4Address();

  // Config
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  PBSInstanceConfig pbs_instance_config_;

  // Cloud Platform Dependency provider factory
  std::shared_ptr<CloudPlatformDependencyFactoryInterface>
      platform_dependency_factory_;

  // Partition
  std::shared_ptr<PartitionLeaseEventSink> partition_lease_event_sink_;
  std::shared_ptr<PBSPartitionManagerInterface> partition_manager_;
  std::shared_ptr<core::PartitionNamespaceInterface> partition_namespace_;
  std::shared_ptr<core::HttpRequestRouterInterface> request_router_;
  std::shared_ptr<core::HttpRequestRouteResolverInterface>
      request_route_resolver_;
  PBSPartition::Dependencies partition_dependencies_;
  std::vector<core::PartitionId> partition_ids_;

  // PBS Virtual Node Ids
  std::vector<core::PartitionId> pbs_vnode_ids_;

  // Executors
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;

  // Misc. Clients
  std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
      instance_client_provider_;
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  // Lease Manager and Leasable Lock
  // NOTE: The 'nosql_database_provider_for_leasable_lock_' is shared between
  // 'vnode' and 'partition' lease managers.
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_leasable_lock_;
  std::shared_ptr<core::LeaseManagerV2Interface>
      partition_lease_manager_service_;
  std::shared_ptr<core::LeaseManagerV2Interface> vnode_lease_manager_service_;
  std::shared_ptr<core::LeaseEventSinkInterface> vnode_lease_event_sink_;

  // Partition Lease Preference Applier
  std::shared_ptr<PartitionLeasePreferenceApplier>
      partition_lease_preference_applier_;

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
      nosql_database_provider_for_background_operations_;
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_live_traffic_;

  // HTTP
  std::shared_ptr<core::HttpServerInterface> http_server_;
  std::shared_ptr<core::HttpServerInterface> health_http_server_;
  std::shared_ptr<core::HttpClientInterface> http1_client_;
  std::shared_ptr<core::HttpClientInterface> http2_client_;
  std::shared_ptr<core::HttpClientInterface> http2_client_for_forwarder_;
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
