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

namespace google::scp::core {
// AWS/GCP cloud region name
static constexpr char kCloudServiceRegion[] = "google_scp_core_cloud_region";
// GCP Project Id (Not project name)
static constexpr char kGcpProjectId[] = "google_scp_gcp_project_id";
// GCP Cloud Spanner
static constexpr char kSpannerInstance[] = "google_scp_spanner_instance_name";
static constexpr char kSpannerDatabase[] = "google_scp_spanner_database_name";
// Skip a log if unable to apply during log recovery
static constexpr char kTransactionManagerSkipFailedLogsInRecovery[] =
    "google_scp_transaction_manager_skip_failed_logs_in_recovery";
static constexpr char kPBSJournalServiceFlushIntervalInMilliseconds[] =
    "google_scp_journal_service_flush_interval_in_milliseconds";
static constexpr char kTransactionTimeoutInSecondsConfigName[] =
    "google_scp_pbs_transaction_timeout_in_seconds";
static constexpr char kTransactionResolutionWithRemoteEnabled[] =
    "google_scp_pbs_transaction_resolution_with_remote_enabled";
static constexpr char kAggregatedMetricIntervalMs[] =
    "google_scp_aggregated_metric_interval_ms";
static constexpr char kHTTPServerRequestRoutingEnabled[] =
    "google_scp_http_server_request_routing_enabled";
static constexpr char kPBSJournalInputStreamEnableBatchReadJournals[] =
    "google_scp_pbs_journal_input_stream_enable_batch_read_journals";
static constexpr char kPBSJournalInputStreamNumberOfJournalsPerBatch[] =
    "google_scp_pbs_journal_input_stream_number_of_journals_per_batch";
static constexpr char kPBSJournalInputStreamNumberOfJournalLogsToReturn[] =
    "google_scp_pbs_journal_input_stream_number_of_journal_logs_to_return";
static constexpr char kPBSAuthorizationEnableSiteBasedAuthorization[] =
    "google_scp_pbs_authorization_enable_site_based_authorization";
}  // namespace google::scp::core
