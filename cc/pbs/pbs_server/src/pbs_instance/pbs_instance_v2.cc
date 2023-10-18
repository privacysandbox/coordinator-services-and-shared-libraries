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

#include "pbs/pbs_server/src/pbs_instance/pbs_instance_v2.h"

// IWYU pragma: no_include <bits/chrono.h>
#include <atomic>
#include <chrono>  // IWYU pragma: keep
#include <cstdlib>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <google/protobuf/repeated_ptr_field.h>

#include "absl/strings/str_cat.h"
#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/operation_dispatcher/src/retry_strategy.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/src/config_provider.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_forwarder/src/http2_forwarder.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/async_context.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/partition_interface.h"
#include "core/interface/partition_manager_interface.h"
#include "core/interface/traffic_forwarder_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "core/interface/transaction_request_router_interface.h"
#include "core/journal_service/src/journal_service.h"
#include "core/lease_manager/src/lease_manager.h"
#include "core/tcp_traffic_forwarder/src/tcp_traffic_forwarder_socat.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "pbs/checkpoint_service/src/checkpoint_service.h"
#include "pbs/front_end_service/src/front_end_service.h"
#include "pbs/health_service/src/health_service.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/front_end_service_interface.h"
#include "pbs/leasable_lock/src/leasable_lock_on_nosql_database.h"
#include "pbs/partition_manager/src/pbs_partition_manager.h"
#include "pbs/partition_namespace/src/pbs_partition_namespace.h"
#include "pbs/partition_request_router/src/http_request_route_resolver_for_partition.h"
#include "pbs/partition_request_router/src/transaction_request_router_for_partition.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_logging.h"
#include "pbs/remote_transaction_manager/src/remote_transaction_manager.h"
#include "pbs/transactions/src/consume_budget_command_factory.h"
#include "pbs/transactions/src/transaction_command_serializer.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

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
using google::scp::core::Http2Forwarder;
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
using google::scp::core::PartitionAddressUri;
using google::scp::core::PartitionId;
using google::scp::core::PartitionMetadata;
using google::scp::core::PartitionType;
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
using google::scp::core::common::TimeProvider;
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
using std::pair;
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

static constexpr char kLocalPartitionAddressUri[] =
    "" /* local partition does not need an address */;

pair<string, string> PBSInstanceV2::GetInstanceIDAndIPv4Address() {
  pair<string, string> instance_id_and_ipv4_pair;
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
      instance_id_and_ipv4_pair.first =
          move(*instance_details.mutable_instance_id());
      instance_id_and_ipv4_pair.second =
          move(*instance_details.mutable_networks(0)
                    ->mutable_private_ipv4_address());
    }
  } else {
    SCP_ERROR(kPBSInstance, kZeroUuid, execution_result,
              "Cannot obtain ID and IP of the instance");
  }
  // If cannot obtain the IP or ID, fallback to the below.
  if (instance_id_and_ipv4_pair.first.empty()) {
    // Use a unique ID. This is for cloudtop runs of PBS.
    instance_id_and_ipv4_pair.first = ToString(Uuid::GenerateUuid());
    SCP_INFO(kPBSInstance, kZeroUuid, "Using Instance ID: '%s'",
             instance_id_and_ipv4_pair.first.c_str());
  }
  if (instance_id_and_ipv4_pair.second.empty()) {
    // If IP address is unavailable, PBS is running on cloudtop
    instance_id_and_ipv4_pair.second = "localhost";
    SCP_INFO(kPBSInstance, kZeroUuid, "Using Instance IPv4 address: '%s'",
             instance_id_and_ipv4_pair.second.c_str());
  }
  return instance_id_and_ipv4_pair;
}

void PBSInstanceV2::PartitionLeaseAcquired() {
  SCP_INFO(kPBSInstance, kZeroUuid, "PBS Global Partition Lease ACQUIRED");
  // Load Partition. If partition cannot be loaded, terminate process.
  // Do the load asynchronously since loading could take a while.
  // Wait atleast for a complete lease duration time before starting to load
  // the partition.
  auto global_partition_id_str = ToString(core::kGlobalPartitionId);
  auto partition_load_execution_timestamp =
      (TimeProvider::GetSteadyTimestampInNanoseconds() +
       std::chrono::seconds(
           pbs_instance_config_.partition_lease_duration_in_seconds))
          .count();
  auto execution_result = async_executor_->ScheduleFor(
      [partition_manager = partition_manager_, global_partition_id_str]() {
        PartitionMetadata partition_metadata(core::kGlobalPartitionId,
                                             PartitionType::Local,
                                             kLocalPartitionAddressUri);

        SCP_INFO(kPBSInstance, kZeroUuid,
                 "Unloading Remote partition (if any) with ID: %s",
                 global_partition_id_str.c_str());
        // Unload partition (remote) if present
        auto execution_result =
            partition_manager->UnloadPartition(partition_metadata);
        if (!execution_result.Successful() &&
            execution_result !=
                FailureExecutionResult(
                    core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)) {
          // Unloading failed due to some other error, we need to terminate to
          // recover from this error.
          SCP_EMERGENCY(kPBSInstance, kZeroUuid, execution_result,
                        "Unloading Remote Partition failed. Terminating PBS");
          std::abort();
        }

        SCP_INFO(kPBSInstance, kZeroUuid, "Loading Local partition with ID: %s",
                 global_partition_id_str.c_str());

        // Load the Local Partition
        execution_result = partition_manager->LoadPartition(partition_metadata);
        if (!execution_result.Successful()) {
          // If load is unsuccessful, we need to act on this (to reduce
          // downtime) by restarting the process.
          SCP_EMERGENCY(kPBSInstance, kZeroUuid, execution_result,
                        "Loading Local Partition failed. Terminating PBS");
          std::abort();
        }
      },
      partition_load_execution_timestamp,
      partition_load_cancellation_callback_);
  if (!execution_result.Successful()) {
    // Unable to schedule, we need to act on this (to reduce downtime) with a
    // restart of the process.
    SCP_EMERGENCY(kPBSInstance, kZeroUuid, execution_result,
                  "Cannot schedule a task to Load Partition. Terminating PBS");
    std::abort();
  }
}

void PBSInstanceV2::PartitionLeaseUnableToAcquire(
    std::optional<LeaseInfo> lease_info) {
  if (!lease_info.has_value()) {
    SCP_INFO(kPBSInstance, kZeroUuid,
             "PBS Global Partition Lease info not available.");
    return;
  }
  SCP_INFO(kPBSInstance, kZeroUuid, "PBS Global Partition Lease NOTACQUIRED");
  // Load Partition, If cannot Load, terminate process.
  PartitionMetadata partition_metadata(core::kGlobalPartitionId,
                                       PartitionType::Remote,
                                       lease_info->service_endpoint_address);
  auto execution_result = partition_manager_->LoadPartition(partition_metadata);
  if (!execution_result.Successful() &&
      (execution_result !=
       FailureExecutionResult(
           core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS))) {
    // Terminate the process
    SCP_EMERGENCY(kPBSInstance, kZeroUuid, execution_result,
                  "Cannot load Remote Partition. Terminating PBS");
    std::abort();
  }
  execution_result =
      partition_manager_->RefreshPartitionAddress(partition_metadata);
  if (!execution_result.Successful()) {
    // Terminate the process
    SCP_EMERGENCY(
        kPBSInstance, kZeroUuid, execution_result,
        "Cannot refresh address on Remote Partition. Terminating PBS");
    std::abort();
  }
  SCP_INFO(kPBSInstance, kZeroUuid,
           "Remote Partition Address Refreshed to '%s'",
           lease_info->service_endpoint_address.c_str());
}

void PBSInstanceV2::PartitionLeaseLost() {
  // Kill the process when lease is lost
  SCP_EMERGENCY(kPBSInstance, kZeroUuid,
                FailureExecutionResult(core::errors::SC_PBS_LEASE_LOST),
                "PBS Global Partition Lease LOST. Terminating PBS");
  std::abort();
}

void PBSInstanceV2::PartitionLeaseRenewed() {
  SCP_INFO(kPBSInstance, kZeroUuid, "PBS Global Partition Lease RENEWED");
}

void PBSInstanceV2::PartitionLeaseTransitionCallback(
    LeaseTransitionType lease_transition_type,
    std::optional<LeaseInfo> lease_info) {
  if (lease_transition_type == LeaseTransitionType::kAcquired) {
    PartitionLeaseAcquired();
  } else if (lease_transition_type == LeaseTransitionType::kLost) {
    PartitionLeaseLost();
  } else if (lease_transition_type == LeaseTransitionType::kNotAcquired) {
    PartitionLeaseUnableToAcquire(lease_info);
  } else if (lease_transition_type == LeaseTransitionType::kRenewed) {
    PartitionLeaseRenewed();
  } else {
    SCP_EMERGENCY(kPBSInstance, kZeroUuid,
                  FailureExecutionResult(
                      core::errors::SC_PBS_SERVICE_UNRECOVERABLE_ERROR),
                  "Unknown Lease transition type. Terminating PBS");
    std::abort();
  }
}

ExecutionResult PBSInstanceV2::ConstructDependencies() noexcept {
  // Core components
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

  auth_token_provider_cache_ =
      platform_dependency_factory_->ConstructAuthorizationTokenProviderCache(
          async_executor_, io_async_executor_, http1_client_);
  nosql_database_provider_for_leasable_lock_ =
      platform_dependency_factory_->ConstructNoSQLDatabaseClient(
          async_executor_for_leasable_lock_nosql_database_,
          io_async_executor_for_leasable_lock_nosql_database_);
  authorization_proxy_ =
      platform_dependency_factory_->ConstructAuthorizationProxyClient(
          async_executor_, http2_client_);
  auth_token_provider_ =
      platform_dependency_factory_->ConstructInstanceAuthorizer(http1_client_);
  instance_client_provider_ =
      platform_dependency_factory_->ConstructInstanceMetadataClient(
          http1_client_, http2_client_, async_executor_, io_async_executor_,
          auth_token_provider_);
  metric_client_ = platform_dependency_factory_->ConstructMetricClient(
      async_executor_, io_async_executor_, instance_client_provider_);
  remote_coordinator_pbs_client_ =
      platform_dependency_factory_->ConstructRemoteCoordinatorPBSClient(
          http2_client_, auth_token_provider_cache_);
  blob_storage_provider_for_journal_service_ =
      platform_dependency_factory_->ConstructBlobStorageClient(
          async_executor_, io_async_executor_);
  blob_storage_provider_for_checkpoint_service_ =
      platform_dependency_factory_->ConstructBlobStorageClient(
          async_executor_, io_async_executor_);
  nosql_database_provider_ =
      platform_dependency_factory_->ConstructNoSQLDatabaseClient(
          async_executor_, io_async_executor_);
  remote_transaction_manager_ =
      make_shared<RemoteTransactionManager>(remote_coordinator_pbs_client_);

  // Partition Dependencies
  partition_dependencies_.async_executor = async_executor_;
  partition_dependencies_.blob_store_provider =
      blob_storage_provider_for_journal_service_;
  partition_dependencies_.blob_store_provider_for_checkpoints =
      blob_storage_provider_for_checkpoint_service_;
  partition_dependencies_.config_provider = config_provider_;
  partition_dependencies_.metric_client = metric_client_;
  partition_dependencies_.nosql_database_provider_for_background_operations =
      nosql_database_provider_;
  partition_dependencies_.nosql_database_provider_for_live_traffic =
      nosql_database_provider_;
  partition_dependencies_.remote_transaction_manager =
      remote_transaction_manager_;

  // Partition
  partition_manager_ = make_shared<PBSPartitionManager>(
      partition_dependencies_,
      pbs_instance_config_.transaction_manager_capacity);
  partitions_set_ = {core::kGlobalPartitionId};
  partition_namespace_ = make_shared<PBSPartitionNamespace>(partitions_set_);

  // HTTP and FrontEndService
  pass_thru_authorization_proxy_ = make_shared<PassThruAuthorizationProxy>();
  core::Http2ServerOptions http2_server_options(
      pbs_instance_config_.http2_server_use_tls,
      pbs_instance_config_.http2_server_private_key_file_path,
      pbs_instance_config_.http2_server_certificate_file_path);
  request_router_ = make_shared<Http2Forwarder>(http2_client_);
  request_route_resolver_ = make_shared<HttpRequestRouteResolverForPartition>(
      partition_namespace_, partition_manager_, config_provider_);
  http_server_ = make_shared<Http2Server>(
      *pbs_instance_config_.host_address, *pbs_instance_config_.host_port,
      pbs_instance_config_.http2server_thread_pool_size, async_executor_,
      authorization_proxy_, request_router_, request_route_resolver_,
      metric_client_, config_provider_, http2_server_options);
  health_http_server_ = make_shared<Http2Server>(
      *pbs_instance_config_.host_address, *pbs_instance_config_.health_port,
      1 /* one thread needed */, async_executor_,
      pass_thru_authorization_proxy_,
      nullptr /* metric_client, no metric recording for health http server */,
      config_provider_, http2_server_options);
  health_service_ = make_shared<HealthService>(
      health_http_server_, config_provider_, async_executor_, metric_client_);
  auto consume_budget_command_factory =
      make_unique<ConsumeBudgetCommandFactory>(
          nullptr /* async executor */, nullptr /* budget key provider */);
  auto transaction_request_router =
      make_unique<TransactionRequestRouterForPartition>(partition_namespace_,
                                                        partition_manager_);
  front_end_service_ = make_shared<FrontEndService>(
      http_server_, async_executor_, move(transaction_request_router),
      move(consume_budget_command_factory), metric_client_, config_provider_);

  // Lease Management for the Global Partition
  auto instance_id_and_ipv4_pair = GetInstanceIDAndIPv4Address();
  string uri_scheme = "http";
  if (http2_server_options.use_tls) {
    uri_scheme = "https";
  }
  auto pbs_endpoint_uri =
      absl::StrCat(uri_scheme, "://", instance_id_and_ipv4_pair.second, ":",
                   *pbs_instance_config_.external_exposed_host_port);
  lease_acquirer_info_.lease_acquirer_id = instance_id_and_ipv4_pair.first;
  lease_acquirer_info_.service_endpoint_address = pbs_endpoint_uri;

  lease_manager_service_ = make_shared<LeaseManager>();
  auto leasable_lock_for_global_partition =
      make_shared<LeasableLockOnNoSQLDatabase>(
          nosql_database_provider_for_leasable_lock_, lease_acquirer_info_,
          *pbs_instance_config_.partition_lease_table_name,
          kPBSPartitionLockTableRowKeyForGlobalPartitionV2,
          pbs_instance_config_.partition_lease_duration_in_seconds);
  return lease_manager_service_->ManageLeaseOnLock(
      leasable_lock_for_global_partition,
      bind(&PBSInstanceV2::PartitionLeaseTransitionCallback, this, _1, _2));
}

ExecutionResult PBSInstanceV2::Init() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV2 Initializing");

  // Read configurations
  ASSIGN_OR_RETURN(pbs_instance_config_,
                   GetPBSInstanceConfigFromConfigProvider(config_provider_));

  // Construct dependencies
  SCP_INFO(kPBSInstance, kZeroUuid, "Constructing Dependencies");
  RETURN_IF_FAILURE(ConstructDependencies());

  // Initializing dependencies
  SCP_INFO(kPBSInstance, kZeroUuid, "Initializing Dependencies");

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
  INIT_PBS_COMPONENT(auth_token_provider_cache_);
  INIT_PBS_COMPONENT(remote_coordinator_pbs_client_);
  INIT_PBS_COMPONENT(remote_transaction_manager_);
  INIT_PBS_COMPONENT(authorization_proxy_);
  INIT_PBS_COMPONENT(blob_storage_provider_for_checkpoint_service_);
  INIT_PBS_COMPONENT(partition_manager_);
  INIT_PBS_COMPONENT(request_router_);
  INIT_PBS_COMPONENT(request_route_resolver_);
  INIT_PBS_COMPONENT(http_server_);
  INIT_PBS_COMPONENT(front_end_service_);
  INIT_PBS_COMPONENT(pass_thru_authorization_proxy_);
  INIT_PBS_COMPONENT(health_http_server_);
  INIT_PBS_COMPONENT(health_service_);
  INIT_PBS_COMPONENT(lease_manager_service_);

  return SuccessExecutionResult();
}

ExecutionResult PBSInstanceV2::Run() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV2 Attempting to Run");

  RUN_PBS_COMPONENT(async_executor_);
  RUN_PBS_COMPONENT(io_async_executor_);
  RUN_PBS_COMPONENT(http2_client_);
  RUN_PBS_COMPONENT(http1_client_);
  RUN_PBS_COMPONENT(instance_client_provider_);
  RUN_PBS_COMPONENT(metric_client_);
  RUN_PBS_COMPONENT(async_executor_for_leasable_lock_nosql_database_);
  RUN_PBS_COMPONENT(io_async_executor_for_leasable_lock_nosql_database_);
  RUN_PBS_COMPONENT(blob_storage_provider_for_journal_service_);
  RUN_PBS_COMPONENT(nosql_database_provider_);
  RUN_PBS_COMPONENT(nosql_database_provider_for_leasable_lock_);
  RUN_PBS_COMPONENT(auth_token_provider_cache_);
  RUN_PBS_COMPONENT(remote_coordinator_pbs_client_);
  RUN_PBS_COMPONENT(remote_transaction_manager_);
  RUN_PBS_COMPONENT(authorization_proxy_);
  RUN_PBS_COMPONENT(blob_storage_provider_for_checkpoint_service_);
  RUN_PBS_COMPONENT(partition_manager_);
  RUN_PBS_COMPONENT(request_router_);
  RUN_PBS_COMPONENT(http_server_);
  RUN_PBS_COMPONENT(front_end_service_);
  RUN_PBS_COMPONENT(pass_thru_authorization_proxy_);
  RUN_PBS_COMPONENT(health_http_server_);
  RUN_PBS_COMPONENT(health_service_);
  RUN_PBS_COMPONENT(lease_manager_service_);

  return SuccessExecutionResult();
}

ExecutionResult PBSInstanceV2::Stop() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV2 Attempting to Stop");

  // Cancel Partiton Loading if any.
  if (partition_load_cancellation_callback_) {
    partition_load_cancellation_callback_();
  }

  STOP_PBS_COMPONENT(lease_manager_service_);
  STOP_PBS_COMPONENT(health_http_server_);
  STOP_PBS_COMPONENT(health_service_);
  STOP_PBS_COMPONENT(pass_thru_authorization_proxy_);
  STOP_PBS_COMPONENT(front_end_service_);
  STOP_PBS_COMPONENT(http_server_);
  STOP_PBS_COMPONENT(request_router_);
  STOP_PBS_COMPONENT(partition_manager_);
  STOP_PBS_COMPONENT(blob_storage_provider_for_checkpoint_service_);
  STOP_PBS_COMPONENT(authorization_proxy_);
  STOP_PBS_COMPONENT(remote_transaction_manager_);
  STOP_PBS_COMPONENT(remote_coordinator_pbs_client_);
  STOP_PBS_COMPONENT(auth_token_provider_cache_);
  STOP_PBS_COMPONENT(nosql_database_provider_for_leasable_lock_);
  STOP_PBS_COMPONENT(nosql_database_provider_);
  STOP_PBS_COMPONENT(blob_storage_provider_for_journal_service_);
  STOP_PBS_COMPONENT(io_async_executor_for_leasable_lock_nosql_database_);
  STOP_PBS_COMPONENT(async_executor_for_leasable_lock_nosql_database_);
  STOP_PBS_COMPONENT(metric_client_);
  STOP_PBS_COMPONENT(instance_client_provider_);
  STOP_PBS_COMPONENT(http1_client_);
  STOP_PBS_COMPONENT(http2_client_);
  STOP_PBS_COMPONENT(io_async_executor_);
  STOP_PBS_COMPONENT(async_executor_);

  return SuccessExecutionResult();
}

}  // namespace google::scp::pbs
