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

namespace google::scp::pbs {
static constexpr char kTotalHttp2ServerThreadsCount[] =
    "google_scp_core_http2server_threads_count";
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
static constexpr char kPrivacyBudgetServiceHostAddress[] =
    "google_scp_pbs_host_address";
static constexpr char kPrivacyBudgetServiceHostPort[] =
    "google_scp_pbs_host_port";
static constexpr char kPrivacyBudgetServiceHealthPort[] =
    "google_scp_pbs_health_port";
static constexpr char kAuthServiceEndpoint[] = "google_scp_pbs_auth_endpoint";
// This is the AWS endpoint used on GCP to authenticate requests that come from
// AWS PBS to GCP PBS via DNS.
static constexpr char kAlternateAuthServiceEndpoint[] =
    "google_scp_pbs_alternate_auth_endpoint";

// Logging
static constexpr char kEnabledLogLevels[] =
    "google_scp_core_enabled_log_levels";
static constexpr char kLogProvider[] = "google_scp_pbs_log_provider";

// HTTP2 Server TLS context
static constexpr char kHttp2ServerUseTls[] =
    "google_scp_pbs_http2_server_use_tls";
static constexpr char kHttp2ServerPrivateKeyFilePath[] =
    "google_scp_pbs_http2_server_private_key_file_path";
static constexpr char kHttp2ServerCertificateFilePath[] =
    "google_scp_pbs_http2_server_certificate_file_path";

// Health service
static constexpr char kPBSHealthServiceEnableMemoryAndStorageCheck[] =
    "google_scp_pbs_health_service_enable_mem_and_storage_check";

// PBS with relaxed consistency
static constexpr char kPBSRelaxedConsistencyEnabled[] =
    "google_scp_pbs_relaxed_consistency_enabled";

// Opentelemetry
static constexpr char kOtelEnabled[] = "google_scp_otel_enabled";
static constexpr char kOtelPrintDataToConsoleEnabled[] =
    "google_scp_otel_print_data_to_console_enabled";

// Container type (cloud_run or compute_engine)
static constexpr char kContainerType[] = "google_scp_pbs_container_type";

// Migration phase for ValueProto column.
static constexpr char kValueProtoMigrationPhase[] =
    "google_scp_pbs_value_proto_migration_phase";

// Migration of PBS to use budget consumer
static constexpr char kEnableBudgetConsumerMigration[] =
    "google_scp_pbs_migration_enable_budget_consumer_migration";

// Migration of PBS to use request response proto
static constexpr char kEnableRequestResponseProtoMigration[] =
    "google_scp_pbs_enable_request_response_proto_migration";

// Stop servering requests with JSON request version 1.0
static constexpr char kEnableStopServingV1Request[] =
    "google_pbs_stop_serving_v1_request";
}  // namespace google::scp::pbs
