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

#include "pbs_instance.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/src/config_provider.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/traffic_forwarder_interface.h"
#include "core/journal_service/src/journal_service.h"
#include "core/lease_manager/src/lease_manager.h"
#include "core/tcp_traffic_forwarder/src/tcp_traffic_forwarder_socat.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "pbs/checkpoint_service/src/checkpoint_service.h"
#include "pbs/front_end_service/src/front_end_service.h"
#include "pbs/front_end_service/src/transaction_request_router.h"
#include "pbs/health_service/src/health_service.h"
#include "pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/leasable_lock/src/leasable_lock_on_nosql_database.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_logging.h"
#include "pbs/remote_transaction_manager/src/remote_transaction_manager.h"
#include "pbs/transactions/src/consume_budget_command_factory.h"
#include "pbs/transactions/src/consume_budget_command_factory_interface.h"
#include "pbs/transactions/src/transaction_command_serializer.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

#if defined(PBS_GCP)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#elif defined(PBS_GCP_INTEGRATION_TEST)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/gcp_integration_test/gcp_integration_test_dependency_factory.h"
#elif defined(PBS_AWS)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/aws/aws_dependency_factory.h"
#elif defined(PBS_AWS_INTEGRATION_TEST)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/aws_integration_test/aws_integration_test_dependency_factory.h"
#elif defined(PBS_LOCAL)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
#endif

#include "error_codes.h"

using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::CheckpointServiceInterface;
using google::scp::core::ConfigProvider;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::Http2Server;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpServerInterface;
using google::scp::core::JournalRecoverRequest;
using google::scp::core::JournalRecoverResponse;
using google::scp::core::JournalService;
using google::scp::core::JournalServiceInterface;
using google::scp::core::kCloudServiceRegion;
using google::scp::core::LeasableLockInterface;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManager;
using google::scp::core::LeaseManagerInterface;
using google::scp::core::LeaseTransitionType;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::PassThruAuthorizationProxy;
using google::scp::core::RemoteTransactionManagerInterface;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TCPTrafficForwarderSocat;
using google::scp::core::TrafficForwarderInterface;
using google::scp::core::TransactionCommandSerializerInterface;
using google::scp::core::TransactionManager;
using google::scp::core::TransactionManagerInterface;
using google::scp::core::TransactionRequestRouterInterface;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::pbs::BudgetKeyProvider;
using google::scp::pbs::BudgetKeyProviderInterface;
using google::scp::pbs::CheckpointService;
using google::scp::pbs::FrontEndService;
using google::scp::pbs::FrontEndServiceInterface;
using google::scp::pbs::HealthService;
using google::scp::pbs::LeasableLockOnNoSQLDatabase;
using google::scp::pbs::RemoteTransactionManager;
using google::scp::pbs::TransactionCommandSerializer;
using std::atomic;
using std::bind;
using std::function;
using std::make_shared;
using std::make_unique;
using std::move;
using std::mutex;
using std::optional;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::thread;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::placeholders::_1;
using std::placeholders::_2;
using std::this_thread::sleep_for;

namespace google::scp::pbs {

ExecutionResult PBSInstance::CreateComponents() noexcept {
  instance_id_ = Uuid::GenerateUuid();

  // Read configurations
  auto pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(config_provider_);
  if (!pbs_instance_config_or.Successful()) {
    return pbs_instance_config_or.result();
  }
  pbs_instance_config_ = *pbs_instance_config_or;

  // Construct foundational components
  async_executor_ = make_shared<AsyncExecutor>(
      pbs_instance_config_.async_executor_thread_pool_size,
      pbs_instance_config_.async_executor_queue_size);
  io_async_executor_ = make_shared<AsyncExecutor>(
      pbs_instance_config_.io_async_executor_thread_pool_size,
      pbs_instance_config_.io_async_executor_queue_size);
  http1_client_ =
      make_shared<Http1CurlClient>(async_executor_, io_async_executor_);
  http2_client_ = make_shared<HttpClient>(async_executor_);

  async_executor_for_leasable_lock_nosql_database_ = make_shared<AsyncExecutor>(
      pbs_instance_config_
          .async_executor_thread_pool_size_for_lease_db_requests,
      pbs_instance_config_.async_executor_queue_size_for_lease_db_requests);
  io_async_executor_for_leasable_lock_nosql_database_ =
      make_shared<AsyncExecutor>(
          pbs_instance_config_
              .async_executor_thread_pool_size_for_lease_db_requests,
          pbs_instance_config_.async_executor_queue_size_for_lease_db_requests);

#if defined(PBS_GCP)
  SCP_DEBUG(kPBSInstance, kZeroUuid, "Running GCP PBS Instance.");
  auto platform_dependency_factory =
      std::make_unique<GcpDependencyFactory>(config_provider_);
#elif defined(PBS_GCP_INTEGRATION_TEST)
  SCP_DEBUG(kPBSInstance, kZeroUuid,
            "Running GCP Integration Test PBS Instance.");
  auto platform_dependency_factory =
      std::make_unique<GcpIntegrationTestDependencyFactory>(config_provider_);
#elif defined(PBS_AWS)
  SCP_DEBUG(kPBSInstance, kZeroUuid, "Running AWS PBS Instance.");
  auto platform_dependency_factory =
      std::make_unique<AwsDependencyFactory>(config_provider_);
#elif defined(PBS_AWS_INTEGRATION_TEST)
  SCP_DEBUG(kPBSInstance, kZeroUuid,
            "Running AWS Integration Test PBS Instance.");
  auto platform_dependency_factory =
      std::make_unique<AwsIntegrationTestDependencyFactory>(config_provider_);
#elif defined(PBS_LOCAL)
  SCP_DEBUG(kPBSInstance, kZeroUuid, "Running Local PBS Instance.");
  auto platform_dependency_factory =
      std::make_unique<LocalDependencyFactory>(config_provider_);
#endif

  // Factory should be initialized before the other components are even
  // constructed.
  INIT_PBS_COMPONENT(platform_dependency_factory);

  // External service clients
  auth_token_provider_cache_ =
      platform_dependency_factory->ConstructAuthorizationTokenProviderCache(
          async_executor_, io_async_executor_, http1_client_);
  nosql_database_provider_ =
      platform_dependency_factory->ConstructNoSQLDatabaseClient(
          async_executor_, io_async_executor_);
  nosql_database_provider_for_leasable_lock_ =
      platform_dependency_factory->ConstructNoSQLDatabaseClient(
          async_executor_for_leasable_lock_nosql_database_,
          io_async_executor_for_leasable_lock_nosql_database_);
  authorization_proxy_ =
      platform_dependency_factory->ConstructAuthorizationProxyClient(
          async_executor_, http2_client_);
  auth_token_provider_ =
      platform_dependency_factory->ConstructInstanceAuthorizer(http1_client_);
  instance_client_provider_ =
      platform_dependency_factory->ConstructInstanceMetadataClient(
          http1_client_, http2_client_, async_executor_, io_async_executor_,
          auth_token_provider_);
  metric_client_ = platform_dependency_factory->ConstructMetricClient(
      async_executor_, io_async_executor_, instance_client_provider_);
  remote_coordinator_pbs_client_ =
      platform_dependency_factory->ConstructRemoteCoordinatorPBSClient(
          http2_client_, auth_token_provider_cache_);
  // Create a separate blob storage provider for checkpoint service to isolate
  // the checkpoint service request connections from that of the journal
  // service.
  blob_storage_provider_for_journal_service_ =
      platform_dependency_factory->ConstructBlobStorageClient(
          async_executor_, io_async_executor_);
  blob_storage_provider_for_checkpoint_service_ =
      platform_dependency_factory->ConstructBlobStorageClient(
          async_executor_, io_async_executor_);

  remote_transaction_manager_ =
      make_shared<RemoteTransactionManager>(remote_coordinator_pbs_client_);
  journal_service_ = make_shared<JournalService>(
      pbs_instance_config_.journal_bucket_name,
      pbs_instance_config_.journal_partition_name, async_executor_,
      blob_storage_provider_for_journal_service_, metric_client_,
      config_provider_);
  // TODO: b/297262889 Make a distinction between the live-traffic and
  // background NoSQL operations.
  budget_key_provider_ = make_shared<BudgetKeyProvider>(
      async_executor_, journal_service_, nosql_database_provider_,
      metric_client_, config_provider_);
  transaction_command_serializer_ = make_shared<TransactionCommandSerializer>(
      async_executor_, budget_key_provider_);
  transaction_manager_ = make_shared<TransactionManager>(
      async_executor_, transaction_command_serializer_, journal_service_,
      remote_transaction_manager_,
      pbs_instance_config_.transaction_manager_capacity, metric_client_,
      config_provider_);

  pass_thru_authorization_proxy_ = make_shared<PassThruAuthorizationProxy>();

  core::Http2ServerOptions http2_server_options(
      pbs_instance_config_.http2_server_use_tls,
      pbs_instance_config_.http2_server_private_key_file_path,
      pbs_instance_config_.http2_server_certificate_file_path);

  http_server_ = make_shared<Http2Server>(
      *pbs_instance_config_.host_address, *pbs_instance_config_.host_port,
      pbs_instance_config_.http2server_thread_pool_size, async_executor_,
      authorization_proxy_, metric_client_, config_provider_,
      http2_server_options);
  health_http_server_ = make_shared<Http2Server>(
      *pbs_instance_config_.host_address, *pbs_instance_config_.health_port,
      1 /* one thread needed */, async_executor_,
      pass_thru_authorization_proxy_,
      nullptr /* metric_client, no metric recording for health http server */,
      config_provider_, http2_server_options);

  health_service_ = make_shared<HealthService>(
      health_http_server_, config_provider_, async_executor_, metric_client_);

  auto consume_budget_command_factory =
      make_unique<ConsumeBudgetCommandFactory>(async_executor_,
                                               budget_key_provider_);
  auto transaction_request_router =
      make_unique<TransactionRequestRouter>(transaction_manager_);
  front_end_service_ = make_shared<FrontEndService>(
      http_server_, async_executor_, move(transaction_request_router),
      move(consume_budget_command_factory), metric_client_, config_provider_);

  checkpoint_service_ = make_shared<CheckpointService>(
      pbs_instance_config_.journal_bucket_name,
      pbs_instance_config_.journal_partition_name, metric_client_,
      config_provider_, journal_service_,
      blob_storage_provider_for_checkpoint_service_);
  traffic_forwarder_ =
      make_shared<TCPTrafficForwarderSocat>(*pbs_instance_config_.host_port);
  lease_manager_service_ = make_shared<LeaseManager>();

  return SuccessExecutionResult();
}

void PBSInstance::InitializeLeaseInformation() noexcept {
  string instance_id;
  string ipv4_address;
  string resource_name;
  auto execution_result =
      instance_client_provider_->GetCurrentInstanceResourceNameSync(
          resource_name);

  if (execution_result.Successful()) {
    InstanceDetails instance_details;
    execution_result =
        instance_client_provider_->GetInstanceDetailsByResourceNameSync(
            resource_name, instance_details);
    if (execution_result.Successful() && !instance_details.networks().empty()) {
      instance_id = move(*instance_details.mutable_instance_id());
      ipv4_address = move(*instance_details.mutable_networks(0)
                               ->mutable_private_ipv4_address());
    }
  }

  if (instance_id.empty()) {
    instance_id = ToString(instance_id_);
    SCP_ERROR(kPBSInstance, kZeroUuid, execution_result,
              "Failed to obtain instance ID from cloud. "
              "Continue with default instance ID.");
  }
  if (ipv4_address.empty()) {
    ipv4_address = "0.0.0.0";
    SCP_ERROR(kPBSInstance, kZeroUuid, execution_result,
              "Failed to obtain instance private ipv4 address from cloud. "
              "Continue with default IP address.");
  }

  lease_info_.lease_acquirer_id = instance_id;
  lease_info_.service_endpoint_address = absl::StrCat(
      ipv4_address, ":", *pbs_instance_config_.external_exposed_host_port);
  SCP_INFO(kPBSInstance, kZeroUuid,
           "Initialized Lease Acquirer information. Acquirer ID: %s Service "
           "Endpoint Address: %s",
           lease_info_.lease_acquirer_id.c_str(),
           lease_info_.service_endpoint_address.c_str());
}

ExecutionResult PBSInstance::Init() noexcept {
  RETURN_IF_FAILURE(CreateComponents());

  SCP_INFO(kPBSInstance, kZeroUuid, "PBS Initializing......");

  INIT_PBS_COMPONENT(async_executor_);
  INIT_PBS_COMPONENT(io_async_executor_);
  INIT_PBS_COMPONENT(http2_client_);
  INIT_PBS_COMPONENT(http1_client_);
  INIT_PBS_COMPONENT(instance_client_provider_);
  INIT_PBS_COMPONENT(metric_client_);
  INIT_PBS_COMPONENT(async_executor_for_leasable_lock_nosql_database_);
  INIT_PBS_COMPONENT(io_async_executor_for_leasable_lock_nosql_database_);
  INIT_PBS_COMPONENT(blob_storage_provider_for_journal_service_);
  INIT_PBS_COMPONENT(nosql_database_provider_);
  INIT_PBS_COMPONENT(nosql_database_provider_for_leasable_lock_);
  INIT_PBS_COMPONENT(journal_service_);
  INIT_PBS_COMPONENT(budget_key_provider_);
  INIT_PBS_COMPONENT(auth_token_provider_cache_);
  INIT_PBS_COMPONENT(remote_coordinator_pbs_client_);
  INIT_PBS_COMPONENT(remote_transaction_manager_);
  INIT_PBS_COMPONENT(transaction_manager_);
  INIT_PBS_COMPONENT(authorization_proxy_);
  INIT_PBS_COMPONENT(http_server_);
  INIT_PBS_COMPONENT(front_end_service_);
  INIT_PBS_COMPONENT(pass_thru_authorization_proxy_);
  INIT_PBS_COMPONENT(health_http_server_);
  INIT_PBS_COMPONENT(health_service_);
  INIT_PBS_COMPONENT(blob_storage_provider_for_checkpoint_service_);
  INIT_PBS_COMPONENT(checkpoint_service_);

  // If PBS runs as part of multi-instance deployment, we need to start
  // Lease Manager and Traffic Forwarder components.
  bool is_multi_instance_mode_disabled_in_config = false;
  auto execution_result =
      config_provider_->Get(kPBSMultiInstanceModeDisabledConfigKey,
                            is_multi_instance_mode_disabled_in_config);
  if (!execution_result.Successful()) {
    // If config is not present, continue with multi-instance mode.
    SCP_INFO(kPBSInstance, kZeroUuid,
             "%s flag not specified. Initializing PBS in multi-instance "
             "deployment "
             "mode",
             kPBSMultiInstanceModeDisabledConfigKey);
  }

  is_multi_instance_mode_ = !is_multi_instance_mode_disabled_in_config;

  if (is_multi_instance_mode_) {
    // Initialize Lease Manager and Traffic Forwarder
    INIT_PBS_COMPONENT(lease_manager_service_);
    INIT_PBS_COMPONENT(traffic_forwarder_);
  }

  SCP_INFO(kPBSInstance, kZeroUuid, "PBS Instance Initialized");

  return SuccessExecutionResult();
}

void PBSInstance::LeaseTransitionFunction(
    shared_ptr<std::atomic<bool>> is_lease_acquired,
    shared_ptr<TrafficForwarderInterface> traffic_forwarder,
    function<void()> process_termination_function,
    LeaseTransitionType lease_transition_type, optional<LeaseInfo> lease_info) {
  if (lease_transition_type == LeaseTransitionType::kAcquired) {
    SCP_INFO(kPBSInstance, kZeroUuid, "Lease ACQUIRED");
    *is_lease_acquired = true;
  } else if (lease_transition_type == LeaseTransitionType::kLost) {
    // Kill the process when lease is lost
    // NOTE: Graceful termination almost always doesn't work correctly
    // leading to two instances holding lease, so going with
    // ungraceful termination.
    SCP_EMERGENCY(kPBSInstance, kZeroUuid,
                  FailureExecutionResult(core::errors::SC_PBS_LEASE_LOST),
                  "Lease LOST. Terminating the process...");
    process_termination_function();
  } else if (lease_transition_type == LeaseTransitionType::kNotAcquired) {
    if (lease_info.has_value()) {
      SCP_INFO(kPBSInstance, kZeroUuid,
               "Lease NOTACQUIRED. Will forward traffic to %s",
               lease_info->service_endpoint_address.c_str());
      auto execution_result = traffic_forwarder->ResetForwardingAddress(
          lease_info->service_endpoint_address);
      if (!execution_result.Successful()) {
        SCP_INFO(kPBSInstance, kZeroUuid,
                 "Unable to reset the Traffic Forwarder's address to: %s. "
                 "Terminating..",
                 lease_info->service_endpoint_address.c_str());
        process_termination_function();
      }
    }
  } else if (lease_transition_type == LeaseTransitionType::kRenewed) {
    SCP_INFO(kPBSInstance, kZeroUuid, "Lease RENEWED");
  } else {
    // Other lease transitions are not consumed.
  }
}

ExecutionResult PBSInstance::RunLeaseManagerAndWaitUntilLeaseIsAcquired(
    shared_ptr<LeaseManagerInterface> lease_manager_service,
    shared_ptr<LeasableLockInterface> leasable_lock,
    shared_ptr<TrafficForwarderInterface> traffic_forwarder,
    function<void()> process_termination_function) {
  auto is_lease_acquired = make_shared<atomic<bool>>(false);
  ExecutionResult execution_result = lease_manager_service->ManageLeaseOnLock(
      leasable_lock,
      bind(&LeaseTransitionFunction, is_lease_acquired, traffic_forwarder,
           process_termination_function, _1, _2));
  if (!execution_result.Successful()) {
    return execution_result;
  }

  RUN_PBS_COMPONENT(lease_manager_service);

  while (!*is_lease_acquired) {
    SCP_INFO(kPBSInstance, kZeroUuid, "Waiting on lease acquisition...");
    sleep_for(seconds(1));
  }
  return SuccessExecutionResult();
}

ExecutionResult PBSInstance::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(core::errors::SC_PBS_SERVICE_ALREADY_RUNNING);
  }

  SCP_INFO(kPBSInstance, kZeroUuid, "Starting PBS components");

  RUN_PBS_COMPONENT(async_executor_);
  RUN_PBS_COMPONENT(io_async_executor_);
  RUN_PBS_COMPONENT(http1_client_);
  RUN_PBS_COMPONENT(http2_client_);
  RUN_PBS_COMPONENT(instance_client_provider_);
  RUN_PBS_COMPONENT(metric_client_);
  RUN_PBS_COMPONENT(pass_thru_authorization_proxy_);
  RUN_PBS_COMPONENT(health_http_server_);
  RUN_PBS_COMPONENT(health_service_);

  if (is_multi_instance_mode_) {
    // Run traffic fowarder until the lease is acquired on the PBS lock.
    RUN_PBS_COMPONENT(traffic_forwarder_);

    SCP_INFO(kPBSInstance, kZeroUuid,
             "Starting lease manager and then will wait on lease acquisition");

    SCP_INFO(kPBSInstance, kZeroUuid, "Using lease duration of '%ld' seconds",
             pbs_instance_config_.partition_lease_duration_in_seconds);

    // The system's availability depends on the Leasable Lock, so any requests
    // that the lock makes to the nosqldatabase must get priority over other
    // ongoing database requests.
    RUN_PBS_COMPONENT(async_executor_for_leasable_lock_nosql_database_);
    RUN_PBS_COMPONENT(io_async_executor_for_leasable_lock_nosql_database_);
    RUN_PBS_COMPONENT(nosql_database_provider_for_leasable_lock_);

    // Initialize Lease information after instance_client_provider and its
    // dependencies run.
    InitializeLeaseInformation();

    auto leaseable_lock = make_shared<LeasableLockOnNoSQLDatabase>(
        nosql_database_provider_for_leasable_lock_, lease_info_,
        *pbs_instance_config_.partition_lease_table_name,
        kPBSPartitionLockTableRowKeyForGlobalPartition,
        pbs_instance_config_.partition_lease_duration_in_seconds);
    auto execution_result = RunLeaseManagerAndWaitUntilLeaseIsAcquired(
        lease_manager_service_, leaseable_lock, traffic_forwarder_, []() {
          SCP_EMERGENCY(kPBSInstance, kZeroUuid,
                        FailureExecutionResult(core::errors::SC_PBS_LEASE_LOST),
                        "Terminating the process.");
          std::abort();
        });
    if (!execution_result.Successful()) {
      SCP_CRITICAL(kPBSInstance, kZeroUuid, execution_result,
                   "Failed to wait on lease acquisition.");
      return execution_result;
    }

    // Stop traffic forwarder as this instance now holds the lease and is
    // allowed to start the PBS service
    SCP_INFO(kPBSInstance, kZeroUuid,
             "Lease acquired, stopping traffic forwarder");
    STOP_PBS_COMPONENT(traffic_forwarder_);

    // Wait for a couple of lease duration cycles to ensure existing lease
    // holders have terminated
    SCP_INFO(kPBSInstance, kZeroUuid,
             "Lease acquired, waiting for %lld milliseconds before starting "
             "log "
             "recovery",
             leaseable_lock->GetConfiguredLeaseDurationInMilliseconds() * 2);
    sleep_for(milliseconds(
        leaseable_lock->GetConfiguredLeaseDurationInMilliseconds() * 2));
  }

  // Start the storage service required for processing the logs
  RUN_PBS_COMPONENT(blob_storage_provider_for_journal_service_);

  SCP_INFO(kPBSInstance, kZeroUuid, "Starting log recovery");

  std::atomic<bool> recovery_completed = false;
  std::atomic<bool> recovery_failed = false;
  AsyncContext<JournalRecoverRequest, JournalRecoverResponse> recovery_context;
  recovery_context.request = make_shared<JournalRecoverRequest>();
  auto activity_id = Uuid::GenerateUuid();
  recovery_context.parent_activity_id = activity_id;
  recovery_context.correlation_id = activity_id;
  recovery_context.callback =
      [&](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
              recovery_context) {
        if (!recovery_context.result.Successful()) {
          SCP_CRITICAL(kPBSInstance, kZeroUuid, recovery_context.result,
                       "Log recovery failed.");
          recovery_failed = true;
        }
        recovery_completed = true;
      };

  // Recovering the service

  // Recovery metrics needs to be separately Run because the journal_service_ is
  // not yet Run().
  RETURN_IF_FAILURE(journal_service_->RunRecoveryMetrics());
  RETURN_IF_FAILURE(journal_service_->Recover(recovery_context));

  while (!recovery_completed) {
    sleep_for(milliseconds(250));
  }

  RETURN_IF_FAILURE(journal_service_->StopRecoveryMetrics());

  if (recovery_failed) {
    return FailureExecutionResult(core::errors::SC_PBS_SERVICE_RECOVERY_FAILED);
  }

  is_running_ = true;

  RUN_PBS_COMPONENT(nosql_database_provider_);
  RUN_PBS_COMPONENT(journal_service_);
  RUN_PBS_COMPONENT(budget_key_provider_);
  RUN_PBS_COMPONENT(auth_token_provider_cache_);
  RUN_PBS_COMPONENT(remote_coordinator_pbs_client_);
  RUN_PBS_COMPONENT(remote_transaction_manager_);
  RUN_PBS_COMPONENT(transaction_manager_);
  RUN_PBS_COMPONENT(authorization_proxy_);
  RUN_PBS_COMPONENT(http_server_);
  RUN_PBS_COMPONENT(front_end_service_);
  RUN_PBS_COMPONENT(blob_storage_provider_for_checkpoint_service_);
  RUN_PBS_COMPONENT(checkpoint_service_);

  SCP_INFO(kPBSInstance, kZeroUuid, "PBS Instance Running");

  return SuccessExecutionResult();
}

ExecutionResult PBSInstance::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(core::errors::SC_PBS_SERVICE_NOT_RUNNING);
  }

  STOP_PBS_COMPONENT(checkpoint_service_);
  STOP_PBS_COMPONENT(blob_storage_provider_for_checkpoint_service_);
  STOP_PBS_COMPONENT(health_service_);
  STOP_PBS_COMPONENT(health_http_server_);
  STOP_PBS_COMPONENT(pass_thru_authorization_proxy_);
  STOP_PBS_COMPONENT(front_end_service_);
  STOP_PBS_COMPONENT(http_server_);
  STOP_PBS_COMPONENT(authorization_proxy_);
  STOP_PBS_COMPONENT(remote_transaction_manager_);
  STOP_PBS_COMPONENT(remote_coordinator_pbs_client_);
  STOP_PBS_COMPONENT(auth_token_provider_cache_);
  STOP_PBS_COMPONENT(transaction_manager_);
  STOP_PBS_COMPONENT(budget_key_provider_);
  STOP_PBS_COMPONENT(journal_service_);
  STOP_PBS_COMPONENT(blob_storage_provider_for_journal_service_);
  STOP_PBS_COMPONENT(nosql_database_provider_);
  STOP_PBS_COMPONENT(metric_client_);
  STOP_PBS_COMPONENT(instance_client_provider_);
  STOP_PBS_COMPONENT(http2_client_);
  STOP_PBS_COMPONENT(http1_client_);
  STOP_PBS_COMPONENT(io_async_executor_);
  STOP_PBS_COMPONENT(async_executor_);

  if (is_multi_instance_mode_) {
    STOP_PBS_COMPONENT(traffic_forwarder_);
    STOP_PBS_COMPONENT(lease_manager_service_);
    STOP_PBS_COMPONENT(nosql_database_provider_for_leasable_lock_);
    STOP_PBS_COMPONENT(async_executor_for_leasable_lock_nosql_database_);
    STOP_PBS_COMPONENT(io_async_executor_for_leasable_lock_nosql_database_);
  }

  is_running_ = false;
  return SuccessExecutionResult();
}

}  // namespace google::scp::pbs
