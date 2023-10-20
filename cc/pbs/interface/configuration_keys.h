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

#include <cstdint>
#include <string>

namespace google::scp::pbs {
static constexpr char kTotalHttp2ServerThreadsCount[] =
    "google_scp_core_http2server_threads_count";
static constexpr char kServiceMetricsNamespace[] =
    "google_scp_pbs_metrics_namespace";
static constexpr char kServiceMetricsBatchPush[] =
    "google_scp_pbs_metrics_batch_push_enabled";
static constexpr char kServiceMetricsBatchTimeDurationMs[] =
    "google_scp_pbs_metrics_batch_time_duration_ms";
static constexpr char kBudgetKeyTableName[] =
    "google_scp_pbs_budget_key_table_name";
static constexpr char kAsyncExecutorQueueSize[] =
    "google_scp_pbs_async_executor_queue_size";
static constexpr char kAsyncExecutorThreadsCount[] =
    "google_scp_pbs_async_executor_threads_count";
static constexpr char kIOAsyncExecutorQueueSize[] =
    "google_scp_pbs_io_async_executor_queue_size";
static constexpr char kIOAsyncExecutorThreadsCount[] =
    "google_scp_pbs_io_async_executor_threads_count";
static constexpr char kTransactionManagerCapacity[] =
    "google_scp_pbs_transaction_manager_capacity";
static constexpr char kJournalServiceBucketName[] =
    "google_scp_pbs_journal_service_bucket_name";
static constexpr char kJournalServicePartitionName[] =
    "google_scp_pbs_journal_service_partition_name";
static constexpr char kPrivacyBudgetServiceHostAddress[] =
    "google_scp_pbs_host_address";
static constexpr char kPrivacyBudgetServiceHostPort[] =
    "google_scp_pbs_host_port";
static constexpr char kPrivacyBudgetServiceExternalExposedHostPort[] =
    "google_scp_pbs_external_exposed_host_port";
static constexpr char kPrivacyBudgetServiceHealthPort[] =
    "google_scp_pbs_health_port";
static constexpr char kAuthServiceEndpoint[] = "google_scp_pbs_auth_endpoint";
static constexpr char kEnableBatchBudgetCommandsPerDayConfigName[] =
    "google_scp_pbs_enable_batch_budget_commands_per_day";
static constexpr char kDisallowNewTransactionRequests[] =
    "google_scp_pbs_disallow_new_transaction_requests";

// PBS multi-instance mode configurations
static constexpr char kPBSPartitionLockTableNameConfigName[] =
    "google_scp_pbs_partition_lock_table_name";
static constexpr char kPBSPartitionLeaseDurationInSeconds[] =
    "google_scp_pbs_partition_lease_duration_in_seconds";
static constexpr char kPBSMultiInstanceModeDisabledConfigKey[] =
    "google_scp_pbs_multi_instance_mode_disabled";
static constexpr char kPBSPartitionLockTableRowKeyForGlobalPartition[] = "0";
static constexpr char kPBSVNodeLockTableNameConfigName[] =
    "google_scp_pbs_vnode_lock_table_name";
static constexpr char kPBSVNodeLeaseDurationInSeconds[] =
    "google_scp_pbs_vnode_lease_duration_in_seconds";

// NOTE: The following is a breaking change if PBSInstance is upgraded to
// PBSInstanceV2. Ensure that the table entry is updated from a "0"
// (kPBSPartitionLockTableRowKeyForGlobalPartition) to
// "00000000-0000-0000-0000-000000000000".
static constexpr char kPBSPartitionLockTableRowKeyForGlobalPartitionV2[] =
    "00000000-0000-0000-0000-000000000000";

// NOTE: Any changes in the following column schema names must be reflected in
// the terraform deployment script.
// See coordinator/terraform/aws/services/distributedpbs_storage/main.tf
static constexpr char kPBSPartitionLockTableLockIdKeyName[] = "LockId";
static constexpr char kPBSPartitionLockTableLeaseOwnerIdAttributeName[] =
    "LeaseOwnerId";
static constexpr char
    kPBSPartitionLockTableLeaseExpirationTimestampAttributeName[] =
        "LeaseExpirationTimestamp";
static constexpr char
    kPBSLockTableLeaseOwnerServiceEndpointAddressAttributeName[] =
        "LeaseOwnerServiceEndpointAddress";
static constexpr char kPBSLockTableLeaseAcquisitionDisallowedAttributeName[] =
    "LeaseAcquisitionDisallowed";

// Partitioning
static constexpr char kPBSPartitioningEnabled[] =
    "google_scp_pbs_partitioning_enabled";
static constexpr char kPBSPartitionIdList[] =
    "google_scp_pbs_partition_id_list";
static constexpr char kPBSVirtualNodeIdList[] =
    "google_scp_pbs_virtual_node_id_list";

// Remote configurations
static constexpr char kRemotePrivacyBudgetServiceHostAddress[] =
    "google_scp_pbs_remote_host_address";
static constexpr char kRemotePrivacyBudgetServiceCloudServiceRegion[] =
    "google_scp_pbs_remote_cloud_region";
static constexpr char kRemotePrivacyBudgetServiceAuthServiceEndpoint[] =
    "google_scp_pbs_remote_auth_endpoint";
static constexpr char kRemotePrivacyBudgetServiceClaimedIdentity[] =
    "google_scp_pbs_remote_claimed_identity";
static constexpr char kRemotePrivacyBudgetServiceAssumeRoleArn[] =
    "google_scp_pbs_remote_assume_role_arn";
static constexpr char kRemotePrivacyBudgetServiceAssumeRoleExternalId[] =
    "google_scp_pbs_remote_assume_role_external_id";

// Logging
static constexpr char kEnabledLogLevels[] =
    "google_scp_core_enabled_log_levels";

// HTTP2 Server TLS context
static constexpr char kHttp2ServerUseTls[] =
    "google_scp_pbs_http2_server_use_tls";
static constexpr char kHttp2ServerPrivateKeyFilePath[] =
    "google_scp_pbs_http2_server_private_key_file_path";
static constexpr char kHttp2ServerCertificateFilePath[] =
    "google_scp_pbs_http2_server_certificate_file_path";

static constexpr char kPBSJournalCheckpointingIntervalInSeconds[] =
    "google_scp_pbs_journal_checkpointing_interval_in_seconds";
static constexpr char
    kPBSJournalCheckpointingMaxJournalEntriesToProcessInEachRun[] =
        "google_scp_pbs_journal_checkpointing_max_entries_to_process_in_each_"
        "run";

// Health service
static constexpr char kPBSHealthServiceEnableMemoryAndStorageCheck[] =
    "google_scp_pbs_health_service_enable_mem_and_storage_check";

// workload generator
static constexpr char kPBSWorkloadGeneratorMaxHttpRetryCount[] =
    "pbs_workload_generator_max_http_retry_count";

}  // namespace google::scp::pbs
