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
#include <unordered_map>

#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {
typedef uint8_t TokenCount;
typedef std::string BudgetKeyName;
typedef core::Timestamp TimeGroup;
typedef size_t ArrayIndex;
typedef core::Timestamp TimeBucket;
static constexpr char kTransactionIdHeader[] = "x-gscp-transaction-id";
static constexpr char kTransactionLastExecutionTimestampHeader[] =
    "x-gscp-transaction-last-execution-timestamp";
static constexpr char kTransactionOriginHeader[] = "x-gscp-transaction-origin";
static constexpr char kTransactionSecretHeader[] = "x-gscp-transaction-secret";
static constexpr char kBeginTransactionPath[] = "/v1/transactions:begin";
static constexpr char kPrepareTransactionPath[] = "/v1/transactions:prepare";
static constexpr char kCommitTransactionPath[] = "/v1/transactions:commit";
static constexpr char kNotifyTransactionPath[] = "/v1/transactions:notify";
static constexpr char kAbortTransactionPath[] = "/v1/transactions:abort";
static constexpr char kEndTransactionPath[] = "/v1/transactions:end";
static constexpr char kStatusTransactionPath[] = "/v1/transactions:status";
static constexpr char kServiceStatusPath[] = "/v1/service:status";

// Metric labels
static constexpr char kMetricLabelFrontEndService[] = "frontend_service";
static constexpr char kMetricLabelBeginTransaction[] = "begin_transaction";
static constexpr char kMetricLabelPrepareTransaction[] = "prepare_transaction";
static constexpr char kMetricLabelCommitTransaction[] = "commit_transaction";
static constexpr char kMetricLabelAbortTransaction[] = "abort_transaction";
static constexpr char kMetricLabelNotifyTransaction[] = "notify_transaction";
static constexpr char kMetricLabelEndTransaction[] = "end_transaction";
static constexpr char kMetricLabelGetStatusTransaction[] =
    "get_status_transaction";
static constexpr char kMetricLabelValueOperator[] = "operator";
static constexpr char kMetricLabelValueCoordinator[] = "coordinator";
static constexpr char kMetricLabelKeyReportingOrigin[] = "reporting_origin";
static constexpr char kMetricLabelTransactionPhase[] = "transaction_phase";
static constexpr char kMetricLabelTransactionStatus[] = "transaction_status";

// Metric names
static constexpr char kMetricNameRequests[] =
    "google.scp.pbs.frontend.requests";
static constexpr char kMetricNameClientErrors[] =
    "google.scp.pbs.frontend.client_errors";
static constexpr char kMetricNameServerErrors[] =
    "google.scp.pbs.frontend.server_errors";
static constexpr char kMetricNameMemoryUsage[] =
    "google.scp.pbs.health.memory_usage";
static constexpr char kMetricNameFileSystemStorageUsage[] =
    "google.scp.pbs.health.filesystem_storage_usage";

// TODO: This must be configurable.
static constexpr int kMaxToken = 1;
}  // namespace google::scp::pbs
