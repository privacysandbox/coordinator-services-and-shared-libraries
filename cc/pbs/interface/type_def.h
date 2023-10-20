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
static constexpr char kMetricLabelFrontEndService[] = "FrontEndService";
static constexpr char kMetricLabelBeginTransaction[] = "BeginTransaction";
static constexpr char kMetricLabelPrepareTransaction[] = "PrepareTransaction";
static constexpr char kMetricLabelCommitTransaction[] = "CommitTransaction";
static constexpr char kMetricLabelAbortTransaction[] = "AbortTransaction";
static constexpr char kMetricLabelNotifyTransaction[] = "NotifyTransaction";
static constexpr char kMetricLabelEndTransaction[] = "EndTransaction";
static constexpr char kMetricLabelGetStatusTransaction[] =
    "GetStatusTransaction";
static constexpr char kMetricLabelValueOperator[] = "Operator";
static constexpr char kMetricLabelValueCoordinator[] = "Coordinator";
static constexpr char kMetricLabelKeyReportingOrigin[] = "ReportingOrigin";

// Metric names
static constexpr char kMetricNameTotalRequest[] = "TotalRequest";
static constexpr char kMetricNameClientError[] = "ClientErrors";
static constexpr char kMetricNameServerError[] = "ServerErrors";

// TODO: This must be configurable.
static constexpr int kMaxToken = 1;
}  // namespace google::scp::pbs
