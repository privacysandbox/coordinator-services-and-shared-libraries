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

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/authorization_proxy_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/credentials_provider_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_server_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/lease_manager_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/token_provider_cache_interface.h"
#include "core/interface/traffic_forwarder_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "pbs/interface/front_end_service_interface.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {

class PBSInstance : public core::ServiceInterface {
 public:
  explicit PBSInstance(
      std::shared_ptr<core::ConfigProviderInterface> config_provider)
      : config_provider_(config_provider) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

 protected:
  static void LeaseTransitionFunction(
      std::shared_ptr<std::atomic<bool>> is_lease_acquired,
      std::shared_ptr<core::TrafficForwarderInterface> traffic_forwarder,
      std::function<void()> process_termination_function,
      core::LeaseTransitionType lease_transition_type,
      std::optional<core::LeaseInfo> lease_info);

  static core::ExecutionResult RunLeaseManagerAndWaitUntilLeaseIsAcquired(
      std::shared_ptr<core::LeaseManagerInterface> lease_manager_service,
      std::shared_ptr<core::LeasableLockInterface> leasable_lock,
      std::shared_ptr<core::TrafficForwarderInterface> traffic_forwarder,
      std::function<void()> process_termination_function);

  void InitializeLeaseInformation() noexcept;

  virtual core::ExecutionResult CreateComponents() noexcept;

  std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
      auth_token_provider_;
  std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
      instance_client_provider_;
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
  std::shared_ptr<core::TokenProviderCacheInterface> auth_token_provider_cache_;
  std::shared_ptr<core::AsyncExecutorInterface>
      async_executor_for_leasable_lock_nosql_database_;
  std::shared_ptr<core::AsyncExecutorInterface>
      io_async_executor_for_leasable_lock_nosql_database_;
  std::shared_ptr<core::BlobStorageProviderInterface>
      blob_storage_provider_for_journal_service_;
  std::shared_ptr<core::BlobStorageProviderInterface>
      blob_storage_provider_for_checkpoint_service_;
  std::shared_ptr<core::JournalServiceInterface> journal_service_;
  std::shared_ptr<pbs::BudgetKeyProviderInterface> budget_key_provider_;
  std::shared_ptr<core::TransactionCommandSerializerInterface>
      transaction_command_serializer_;
  std::shared_ptr<core::TransactionManagerInterface> transaction_manager_;
  std::shared_ptr<core::RemoteTransactionManagerInterface>
      remote_transaction_manager_;
  std::shared_ptr<core::AuthorizationProxyInterface> authorization_proxy_;
  std::shared_ptr<core::AuthorizationProxyInterface>
      pass_thru_authorization_proxy_;
  std::shared_ptr<core::HttpClientInterface> http1_client_;
  std::shared_ptr<core::HttpClientInterface> http2_client_;
  std::shared_ptr<core::HttpServerInterface> http_server_;
  std::shared_ptr<core::HttpServerInterface> health_http_server_;
  std::shared_ptr<pbs::FrontEndServiceInterface> front_end_service_;
  std::shared_ptr<core::ServiceInterface> health_service_;
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_;
  std::shared_ptr<core::NoSQLDatabaseProviderInterface>
      nosql_database_provider_for_leasable_lock_;
  std::shared_ptr<core::CheckpointServiceInterface> checkpoint_service_;
  std::shared_ptr<core::LeaseManagerInterface> lease_manager_service_;
  std::shared_ptr<core::TrafficForwarderInterface> traffic_forwarder_;
  std::shared_ptr<core::CredentialsProviderInterface> credentials_provider_;
  std::shared_ptr<pbs::PrivacyBudgetServiceClientInterface>
      remote_coordinator_pbs_client_;

  bool is_multi_instance_mode_ = true;
  bool is_running_ = false;
  core::LeaseInfo lease_info_;
  core::common::Uuid instance_id_;

  PBSInstanceConfig pbs_instance_config_;
};

}  // namespace google::scp::pbs
