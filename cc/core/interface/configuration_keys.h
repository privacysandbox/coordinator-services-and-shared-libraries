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
    "google_scp_journal_service_flush_inteval_in_milliseconds";
}  // namespace google::scp::core
