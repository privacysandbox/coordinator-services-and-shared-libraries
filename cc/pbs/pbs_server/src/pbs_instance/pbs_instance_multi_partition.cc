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

#include "pbs/pbs_server/src/pbs_instance/pbs_instance_multi_partition.h"

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
#include "absl/strings/str_split.h"
#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/src/config_provider.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/http2_client/src/http2_client.h"
#include "core/http2_forwarder/src/http2_forwarder.h"
#include "core/http2_server/src/http2_server.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/traffic_forwarder_interface.h"
#include "core/journal_service/src/journal_service.h"
#include "core/lease_manager/src/v2/component_lifecycle_lease_event_sink.h"
#include "core/lease_manager/src/v2/lease_manager_v2.h"
#include "core/lease_manager/src/v2/lease_refresher_factory.h"
#include "core/tcp_traffic_forwarder/src/tcp_traffic_forwarder_socat.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "pbs/checkpoint_service/src/checkpoint_service.h"
#include "pbs/front_end_service/src/front_end_service.h"
#include "pbs/front_end_service/src/transaction_request_router.h"
#include "pbs/health_service/src/health_service.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/leasable_lock/src/leasable_lock_on_nosql_database.h"
#include "pbs/partition_lease_event_sink/src/partition_lease_event_sink.h"
#include "pbs/partition_request_router/src/http_request_route_resolver_for_partition.h"
#include "pbs/partition_request_router/src/transaction_request_router_for_partition.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_logging.h"
#include "pbs/remote_transaction_manager/src/remote_transaction_manager.h"
#include "pbs/transactions/src/consume_budget_command_factory.h"
#include "pbs/transactions/src/consume_budget_command_factory_interface.h"
#include "pbs/transactions/src/transaction_command_serializer.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

#include "error_codes.h"
#include "synchronous_executor.h"

using absl::StrCat;
using absl::StrSplit;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::CheckpointServiceInterface;
using google::scp::core::ComponentLifecycleLeaseEventSink;
using google::scp::core::ConfigProvider;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::Http2Forwarder;
using google::scp::core::Http2Server;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpClientOptions;
using google::scp::core::HttpServerInterface;
using google::scp::core::JournalRecoverRequest;
using google::scp::core::JournalRecoverResponse;
using google::scp::core::JournalService;
using google::scp::core::JournalServiceInterface;
using google::scp::core::kCloudServiceRegion;
using google::scp::core::LeasableLockId;
using google::scp::core::LeasableLockInterface;
using google::scp::core::LeaseAcquisitionPreference;
using google::scp::core::LeaseAcquisitionPreferenceInterface;
using google::scp::core::LeaseEventSinkInterface;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManagerInterface;
using google::scp::core::LeaseManagerV2;
using google::scp::core::LeaseRefresherFactory;
using google::scp::core::LeaseReleaseNotificationInterface;
using google::scp::core::LeaseStatisticsInterface;
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
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::ToString;
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
using std::dynamic_pointer_cast;
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
using std::vector;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::placeholders::_1;
using std::placeholders::_2;
using std::this_thread::sleep_for;

// Forwarder should not retry for too long since the request's intended
// destination might have changed by the time the retries are happening. Giving
// up early will let the request retried at source and land potentially on a
// different instance resolving the issue faster.
static constexpr size_t kForwarderRetryStrategyMaxRetries = 2;
static constexpr size_t kForwarderRetryStrategyDelayInMs = 101;
// The read timeout is kept quite long to ensure the connections are kept alive
// for longer time between the instances even if there is not traffic flowing.
static constexpr size_t kForwarderHttpConnectionReadTimeoutInSeconds = 120;
// The connections count from this instance to the other instances. Should be a
// high number to multiplex traffic onto available connections.
static constexpr size_t kForwarderConnectionsPerTargetHost = 20;

namespace google::scp::pbs {

static ExecutionResultOr<vector<PartitionId>> GetPartitionIds(
    const shared_ptr<ConfigProviderInterface>& config_provider) {
  vector<PartitionId> partition_ids;
  string partition_id_list;
  config_provider->Get(kPBSPartitionIdList,
                       partition_id_list);  // Ignore error.
  if (!partition_id_list.empty()) {
    vector<string> partition_id_strings = StrSplit(partition_id_list, ',');
    partition_ids.reserve(partition_id_strings.size());
    for (const auto& partition_id_string : partition_id_strings) {
      Uuid partition_id;
      RETURN_IF_FAILURE(FromString(partition_id_string, partition_id));
      partition_ids.push_back(partition_id);
    }
  }
  return partition_ids;
}

pair<string, string> PBSInstanceMultiPartition::GetInstanceIDAndIPv4Address() {
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

/**
 * @brief Represents IDs of virtual nodes on which PBS will run.
 * These IDs do not represent the real physical infrastructure IDs such as
 * Virtual Machine IDs.
 *
 * @return ExecutionResultOr<vector<Uuid>>
 */
static ExecutionResultOr<vector<Uuid>> GetVirtualNodeIds(
    shared_ptr<ConfigProviderInterface>& config_provider) {
  vector<Uuid> vnode_ids;
  string vnode_id_list;
  config_provider->Get(kPBSVirtualNodeIdList,
                       vnode_id_list);  // Ignore error.
  if (!vnode_id_list.empty()) {
    vector<string> vnode_id_strings = StrSplit(vnode_id_list, ',');
    for (const auto& vnode_id_string : vnode_id_strings) {
      Uuid vnode_id;
      RETURN_IF_FAILURE(FromString(vnode_id_string, vnode_id));
      vnode_ids.push_back(vnode_id);
    }
  }
  return vnode_ids;
}

ExecutionResult PBSInstanceMultiPartition::ConstructDependencies() noexcept {
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

  http2_client_for_forwarder_ = make_shared<HttpClient>(
      async_executor_,
      HttpClientOptions{RetryStrategyOptions{RetryStrategyType::Exponential,
                                             kForwarderRetryStrategyDelayInMs,
                                             kForwarderRetryStrategyMaxRetries},
                        kForwarderConnectionsPerTargetHost,
                        kForwarderHttpConnectionReadTimeoutInSeconds});

  auth_token_provider_cache_ =
      platform_dependency_factory_->ConstructAuthorizationTokenProviderCache(
          async_executor_, io_async_executor_, http1_client_);
  // TODO: b/279493757 Add Support for Synchronous API on NoSQLDatabaseProvider.
  // SynchronousExecutor is a temporary solution only. SynchronousExecutor does
  // not need Init(), Run() and Stop().
  nosql_database_provider_for_leasable_lock_ =
      platform_dependency_factory_->ConstructNoSQLDatabaseClient(
          make_shared<SynchronousExecutor>(),
          make_shared<SynchronousExecutor>());
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

  // There are 3 levels of priority in executing IO tasks, namely Normal, High,
  // and Urgent in the order of increasing priority.
  // There are 4 kind of operations PBS performs:
  // 1. Blob Write Operations (For Writing Journal Blob, For Writing Checkpoint
  // Blob). This operation is critical for the service to function smoothly so
  // these are set at Urgent priority.
  // 2. Budget Key Get Operation. This operation is issued by live-traffic to
  // get a certain budget key from disk into memory and this is given a high
  // priority than the 3. below.
  // 3. Budget Key Put Operation. This operation is issued by garbage collection
  // operations that happen periodically, and can be deprioritized compared to
  // live-traffic's Get operations, so this is given a Normal priority.

  blob_storage_provider_for_journal_service_ =
      platform_dependency_factory_->ConstructBlobStorageClient(
          async_executor_, io_async_executor_,
          kDefaultAsyncPriorityForCallbackExecution, AsyncPriority::Urgent);
  blob_storage_provider_for_checkpoint_service_ =
      platform_dependency_factory_->ConstructBlobStorageClient(
          async_executor_, io_async_executor_,
          kDefaultAsyncPriorityForCallbackExecution, AsyncPriority::Urgent);
  nosql_database_provider_for_background_operations_ =
      platform_dependency_factory_->ConstructNoSQLDatabaseClient(
          async_executor_, io_async_executor_,
          kDefaultAsyncPriorityForCallbackExecution, AsyncPriority::Normal);
  nosql_database_provider_for_live_traffic_ =
      platform_dependency_factory_->ConstructNoSQLDatabaseClient(
          async_executor_, io_async_executor_,
          kDefaultAsyncPriorityForCallbackExecution, AsyncPriority::High);
  remote_transaction_manager_ =
      make_shared<RemoteTransactionManager>(remote_coordinator_pbs_client_);

  // Two Lease Managers
  // 1. Partition Lease Manager
  // 2. Virtual Node Lease Manager
  auto lease_refresher_factory = make_shared<LeaseRefresherFactory>();
  auto partition_lease_manager_service =
      make_shared<LeaseManagerV2>(lease_refresher_factory);
  partition_lease_manager_service_ = partition_lease_manager_service;
  auto vnode_lease_manager_service = make_shared<LeaseManagerV2>(
      lease_refresher_factory,
      LeaseAcquisitionPreference{1 /* single vnode */,
                                 {} /* no specific preference */});
  vnode_lease_manager_service_ = vnode_lease_manager_service;

  // Partition Dependencies
  partition_dependencies_.async_executor = async_executor_;
  partition_dependencies_.blob_store_provider =
      blob_storage_provider_for_journal_service_;
  partition_dependencies_.blob_store_provider_for_checkpoints =
      blob_storage_provider_for_checkpoint_service_;
  partition_dependencies_.config_provider = config_provider_;
  partition_dependencies_.metric_client = metric_client_;
  partition_dependencies_.nosql_database_provider_for_live_traffic =
      nosql_database_provider_for_live_traffic_;
  partition_dependencies_.nosql_database_provider_for_background_operations =
      nosql_database_provider_for_background_operations_;
  partition_dependencies_.remote_transaction_manager =
      remote_transaction_manager_;

  // Partition
  partition_manager_ = make_shared<PBSPartitionManager>(
      partition_dependencies_,
      pbs_instance_config_.transaction_manager_capacity);
  partition_lease_event_sink_ = make_shared<PartitionLeaseEventSink>(
      partition_manager_, async_executor_,
      dynamic_pointer_cast<LeaseReleaseNotificationInterface>(
          partition_lease_manager_service),
      metric_client_, config_provider_,
      pbs_instance_config_.partition_lease_duration_in_seconds);
  partition_namespace_ = make_shared<PBSPartitionNamespace>(partition_ids_);

  // Partition Lease Preference Applier
  partition_lease_preference_applier_ =
      make_shared<PartitionLeasePreferenceApplier>(
          partition_ids_.size(),
          dynamic_pointer_cast<LeaseStatisticsInterface>(
              vnode_lease_manager_service),
          dynamic_pointer_cast<LeaseAcquisitionPreferenceInterface>(
              partition_lease_manager_service));
  vnode_lease_event_sink_ = make_shared<ComponentLifecycleLeaseEventSink>(
      partition_lease_preference_applier_);

  // HTTP and FrontEndService
  pass_thru_authorization_proxy_ = make_shared<PassThruAuthorizationProxy>();
  core::Http2ServerOptions http2_server_options(
      pbs_instance_config_.http2_server_use_tls,
      pbs_instance_config_.http2_server_private_key_file_path,
      pbs_instance_config_.http2_server_certificate_file_path);
  request_router_ = make_shared<Http2Forwarder>(http2_client_for_forwarder_);
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

  return SuccessExecutionResult();
}

shared_ptr<LeasableLockInterface>
PBSInstanceMultiPartition::CreateLeasableLockForPartition(
    const PartitionId& partition_id, const LeaseInfo& lease_acquirer_info) {
  // Lock identifier is same as the Partition identifier
  // NOTE: nosql_database_provider_for_leasable_lock_ is shared between VNode
  // and Partition LeasableLocks.
  return make_shared<LeasableLockOnNoSQLDatabase>(
      nosql_database_provider_for_leasable_lock_, lease_acquirer_info,
      *pbs_instance_config_.partition_lease_table_name, ToString(partition_id),
      pbs_instance_config_.partition_lease_duration_in_seconds);
}

shared_ptr<LeasableLockInterface>
PBSInstanceMultiPartition::CreateLeasableLockForVNode(
    const Uuid& vnode_id, const LeaseInfo& lease_acquirer_info) {
  // Lock identifier is same as the VNode ID
  // NOTE: nosql_database_provider_for_leasable_lock_ is shared between VNode
  // and Partition LeasableLocks.
  return make_shared<LeasableLockOnNoSQLDatabase>(
      nosql_database_provider_for_leasable_lock_, lease_acquirer_info,
      *pbs_instance_config_.vnode_lease_table_name, ToString(vnode_id),
      pbs_instance_config_.vnode_lease_duration_in_seconds);
}

ExecutionResult PBSInstanceMultiPartition::Init() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceMultiPartition Initializing");

  // Read configurations
  ASSIGN_OR_RETURN(pbs_instance_config_,
                   GetPBSInstanceConfigFromConfigProvider(config_provider_));

  // Read configurations of Partition Ids and PBS Virtual Node Ids
  ASSIGN_OR_RETURN(partition_ids_, GetPartitionIds(config_provider_));
  ASSIGN_OR_RETURN(pbs_vnode_ids_, GetVirtualNodeIds(config_provider_));

  SCP_INFO(kPBSInstance, kZeroUuid,
           "Init PBS with '%llu' partitions, and '%llu' PBS virtual nodes",
           partition_ids_.size(), pbs_vnode_ids_.size());

  // Construct dependencies
  SCP_INFO(kPBSInstance, kZeroUuid, "Constructing Dependencies");
  RETURN_IF_FAILURE(ConstructDependencies());

  // Initializing dependencies
  SCP_INFO(kPBSInstance, kZeroUuid, "Initializing Dependencies");

  INIT_PBS_COMPONENT(async_executor_);
  INIT_PBS_COMPONENT(io_async_executor_);
  INIT_PBS_COMPONENT(http2_client_);
  INIT_PBS_COMPONENT(http2_client_for_forwarder_);
  INIT_PBS_COMPONENT(http1_client_);
  INIT_PBS_COMPONENT(instance_client_provider_);
  INIT_PBS_COMPONENT(metric_client_);
  INIT_PBS_COMPONENT(blob_storage_provider_for_journal_service_);
  INIT_PBS_COMPONENT(nosql_database_provider_for_live_traffic_);
  INIT_PBS_COMPONENT(nosql_database_provider_for_background_operations_);
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
  INIT_PBS_COMPONENT(partition_lease_event_sink_);
  INIT_PBS_COMPONENT(partition_lease_manager_service_);
  INIT_PBS_COMPONENT(vnode_lease_manager_service_);
  INIT_PBS_COMPONENT(partition_lease_preference_applier_);

  return SuccessExecutionResult();
}

ExecutionResult PBSInstanceMultiPartition::Run() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid,
           "PBSInstanceMultiPartition Attempting to Run");

  RUN_PBS_COMPONENT(async_executor_);
  RUN_PBS_COMPONENT(io_async_executor_);
  RUN_PBS_COMPONENT(http2_client_);
  RUN_PBS_COMPONENT(http2_client_for_forwarder_);
  RUN_PBS_COMPONENT(http1_client_);
  RUN_PBS_COMPONENT(instance_client_provider_);
  RUN_PBS_COMPONENT(metric_client_);
  RUN_PBS_COMPONENT(blob_storage_provider_for_journal_service_);
  RUN_PBS_COMPONENT(nosql_database_provider_for_background_operations_);
  RUN_PBS_COMPONENT(nosql_database_provider_for_live_traffic_);
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
  RUN_PBS_COMPONENT(partition_lease_event_sink_);

  // Lease Manager
  auto [instance_id, instance_ip] = GetInstanceIDAndIPv4Address();
  string uri_scheme = "http";
  if (pbs_instance_config_.http2_server_use_tls) {
    uri_scheme = "https";
  }
  auto pbs_endpoint_uri =
      StrCat(uri_scheme, "://", instance_ip, ":",
             *pbs_instance_config_.external_exposed_host_port);
  LeaseInfo lease_acquirer_info{instance_id, pbs_endpoint_uri};
  SCP_INFO(kPBSInstance, kZeroUuid,
           "Initializing Lease Manager. Instance ID: "
           "'%s', Instance IP: '%s'",
           lease_acquirer_info.lease_acquirer_id.c_str(),
           lease_acquirer_info.service_endpoint_address.c_str());

  // Partition Lease Manager.
  // Add partition Ids to lease manager to manage leases on them.
  // Lease event sink is the partition manager.
  for (const auto& partition_id : partition_ids_) {
    RETURN_IF_FAILURE(partition_lease_manager_service_->ManageLeaseOnLock(
        partition_id,
        CreateLeasableLockForPartition(partition_id, lease_acquirer_info),
        partition_lease_event_sink_));
  }
  RUN_PBS_COMPONENT(partition_lease_manager_service_);

  // Virtual Node Lease Manager.
  // Add Virtual Node Ids to lease manager to manage leases on them.
  // Lease event sink is the Vnode Lease Event Sink.
  for (const auto& pbs_vnode_id : pbs_vnode_ids_) {
    RETURN_IF_FAILURE(vnode_lease_manager_service_->ManageLeaseOnLock(
        pbs_vnode_id,
        CreateLeasableLockForVNode(pbs_vnode_id, lease_acquirer_info),
        vnode_lease_event_sink_));
  }
  RUN_PBS_COMPONENT(vnode_lease_manager_service_);

  return SuccessExecutionResult();
}

ExecutionResult PBSInstanceMultiPartition::Stop() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid,
           "PBSInstanceMultiPartition Attempting to Stop");

  // Cancel Partiton Loading if any.
  if (partition_load_cancellation_callback_) {
    partition_load_cancellation_callback_();
  }

  STOP_PBS_COMPONENT(partition_lease_preference_applier_);
  STOP_PBS_COMPONENT(vnode_lease_manager_service_);
  STOP_PBS_COMPONENT(partition_lease_manager_service_);
  STOP_PBS_COMPONENT(partition_lease_event_sink_);
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
  STOP_PBS_COMPONENT(nosql_database_provider_for_background_operations_);
  STOP_PBS_COMPONENT(nosql_database_provider_for_live_traffic_);
  STOP_PBS_COMPONENT(blob_storage_provider_for_journal_service_);
  STOP_PBS_COMPONENT(metric_client_);
  STOP_PBS_COMPONENT(instance_client_provider_);
  STOP_PBS_COMPONENT(http1_client_);
  STOP_PBS_COMPONENT(http2_client_);
  STOP_PBS_COMPONENT(http2_client_for_forwarder_);
  STOP_PBS_COMPONENT(io_async_executor_);
  STOP_PBS_COMPONENT(async_executor_);

  return SuccessExecutionResult();
}

}  // namespace google::scp::pbs
