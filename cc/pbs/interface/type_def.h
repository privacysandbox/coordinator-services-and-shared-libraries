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

#include "cc/core/interface/type_def.h"

namespace privacy_sandbox::pbs {
typedef uint8_t TokenCount;
typedef std::string BudgetKeyName;
typedef ::privacy_sandbox::pbs_common::Timestamp TimeGroup;
typedef size_t ArrayIndex;
typedef ::privacy_sandbox::pbs_common::Timestamp TimeBucket;

static constexpr char kTransactionIdHeader[] = "x-gscp-transaction-id";
static constexpr char kTransactionOriginHeader[] = "x-gscp-transaction-origin";
static constexpr char kBeginTransactionPath[] = "/v1/transactions:begin";
static constexpr char kPrepareTransactionPath[] = "/v1/transactions:prepare";
static constexpr char kCommitTransactionPath[] = "/v1/transactions:commit";
static constexpr char kNotifyTransactionPath[] = "/v1/transactions:notify";
static constexpr char kAbortTransactionPath[] = "/v1/transactions:abort";
static constexpr char kEndTransactionPath[] = "/v1/transactions:end";
static constexpr char kStatusTransactionPath[] = "/v1/transactions:status";
static constexpr char kServiceStatusPath[] = "/v1/service:status";
static constexpr char kStatusHealthCheckPath[] =
    "/v1/transactions:health-check";
static constexpr char kStatusConsumeBudgetPath[] =
    "/v1/transactions:consume-budget";

// Meter
inline constexpr absl::string_view kFrontEndServiceV2Meter =
    "Frontend Service v2";

// Metric names
static constexpr char kSuccessfulBudgetConsumed[] =
    "google.scp.pbs.frontend.successful_budget_consumed";
static constexpr char kKeysPerTransaction[] =
    "google.scp.pbs.frontend.keys_per_transaction";
static constexpr char kMetricNameMemoryUsage[] =
    "google.scp.pbs.health.memory_usage";
static constexpr char kMetricNameFileSystemStorageUsage[] =
    "google.scp.pbs.health.filesystem_storage_usage";
inline constexpr absl::string_view kBudgetExhausted =
    "google.scp.pbs.consume_budget.budget_exhausted";

// Metric labels
static constexpr char kMetricLabelFrontEndService[] = "frontend_service";
static constexpr char kMetricLabelBeginTransaction[] = "BEGIN";
static constexpr char kMetricLabelPrepareTransaction[] = "PREPARE";
static constexpr char kMetricLabelCommitTransaction[] = "COMMIT";
static constexpr char kMetricLabelAbortTransaction[] = "ABORT";
static constexpr char kMetricLabelNotifyTransaction[] = "NOTIFY";
static constexpr char kMetricLabelEndTransaction[] = "END";
static constexpr char kMetricLabelGetStatusTransaction[] = "GET_STATUS";
static constexpr char kMetricLabelValueOperator[] = "OPERATOR";
static constexpr char kMetricLabelValueCoordinator[] = "COORDINATOR";
static constexpr char kMetricLabelKeyReportingOrigin[] = "reporting_origin";
static constexpr char kMetricLabelTransactionPhase[] = "transaction_phase";
inline constexpr absl::string_view kErrorReasonLabel = "pbs.error_reason";
}  // namespace privacy_sandbox::pbs
