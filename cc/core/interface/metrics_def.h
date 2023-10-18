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

/**
 * @brief
 * Journal Service Metrics
 *  Metric Name: kMetricNameRecoverCount
 *      Events: kMetricEventNameLogCount
 *
 *  Metric Name: kMetricNameJournalOutputStream
 *      Events: kMetricEventJournalOutputCountWriteJournalScheduledCount
 *      Events: kMetricEventJournalOutputCountWriteJournalSuccessCount
 *      Events: kMetricEventJournalOutputCountWriteJournalFailureCount
 */

static constexpr char
    kMetricComponentNameAndPartitionNamePrefixForJournalService[] =
        "JournalService for Partition ";
static constexpr char kMetricNameRecoverExecutionTime[] =
    "JournalRecoveryExecutionTimeMs";
static constexpr char kMetricNameRecoverCount[] = "JournalRecoveryCounter";
static constexpr char kMetricNameJournalOutputStream[] =
    "JournalOutputStreamCounter";
static constexpr char kMetricMethodRecover[] = "Recover";
static constexpr char kMetricMethodOutputStream[] = "OutputStream";
static constexpr char kMetricEventNameLogCount[] = "LogCount";
static constexpr char
    kMetricEventJournalOutputCountWriteJournalScheduledCount[] =
        "WriteJournal Scheduled";
static constexpr char kMetricEventJournalOutputCountWriteJournalSuccessCount[] =
    "WriteJournal Success";
static constexpr char kMetricEventJournalOutputCountWriteJournalFailureCount[] =
    "WriteJournal Failure";

/**
 * @brief
 * Transaction Manager Metrics
 *  Metric Name: kMetricNameActiveTransaction
 *      Events: kMetricEventReceivedTransaction
 *      Events: kMetricEventFinishedTransaction
 */

static constexpr char
    kMetricComponentNameAndPartitionNamePrefixForTransactionManager[] =
        "TransactionManager for Partition ";
static constexpr char kMetricNameActiveTransaction[] = "ActiveTransaction";
static constexpr char kMetricEventReceivedTransaction[] = "ReceivedTransaction";
static constexpr char kMetricEventFinishedTransaction[] = "FinishedTransaction";

/**
 * @brief
 * HTTP Server Metrics
 *  Metric Name: kMetricNameHttpRequest
 *      Events: kMetricEventHttp2xxLocal
 *      Events: kMetricEventHttp4xxLocal
 *      Events: kMetricEventHttp5xxLocal
 *      Events: kMetricEventHttp2xxForwarded
 *      Events: kMetricEventHttp4xxForwarded
 *      Events: kMetricEventHttp5xxForwarded
 */

static constexpr char kMetricNameHttpRequest[] = "HttpRequest";
static constexpr char kMetricEventHttpUnableToResolveRoute[] =
    "Can't Resolve Route 5xx";
static constexpr char kMetricEventHttp2xxLocal[] = "Non-Forwarded 2xx";
static constexpr char kMetricEventHttp4xxLocal[] = "Non-Forwarded 4xx";
static constexpr char kMetricEventHttp5xxLocal[] = "Non-Forwarded 5xx";
static constexpr char kMetricEventHttp2xxForwarded[] = "Forwarded 2xx";
static constexpr char kMetricEventHttp4xxForwarded[] = "Forwarded 4xx";
static constexpr char kMetricEventHttp5xxForwarded[] = "Forwarded 5xx";

}  // namespace google::scp::core
