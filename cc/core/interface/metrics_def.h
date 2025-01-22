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
 *  Metric Name: kMetricNameJournalRecoveryTime
 *
 *  Metric Name: kMetricNameJournalRecoveryCount
 *      Events: kMetricEventNameLogCount
 *
 *  Metric Name: kMetricNameJournalScheduledOutputStreamCount
 *
 *  Metric Name: kMetricNameJournalOutputStreamCount
 *      Events: kMetricEventJournalOutputCountWriteJournalScheduledCount
 *      Events: kMetricEventJournalOutputCountWriteJournalSuccessCount
 *      Events: kMetricEventJournalOutputCountWriteJournalFailureCount
 *      Labels: kMetricLabelJournalWriteSuccess
 */

static constexpr char kMetricNameJournalRecoveryTime[] =
    "google.scp.pbs.journal.recovery_time";
static constexpr char kMetricNameJournalRecoveryCount[] =
    "google.scp.pbs.journal.recovery_count";
static constexpr char kMetricNameJournalScheduledOutputStreamCount[] =
    "google.scp.pbs.journal.scheduled_output_stream_count";
static constexpr char kMetricNameJournalOutputStreamCount[] =
    "google.scp.pbs.journal.output_stream_count";

static constexpr char kMetricLabelJournalWriteSuccess[] = "write_success";

/**
 * @brief
 * Transaction Manager Metrics
 *  Metric Name: kMetricNameActiveTransactions
 *      Labels: kMetricLabelPartitionId
 *      Events: kMetricEventReceivedTransaction
 *      Events: kMetricEventFinishedTransaction
 *
 *  Metric Name: kMetricNameReceivedTransactions
 *
 *  Metric Name: kMetricNameFinishedTransactions
 */

static constexpr char
    kMetricComponentNameAndPartitionNamePrefixForTransactionManager[] =
        "TransactionManager for Partition ";
static constexpr char kMetricNameActiveTransactions[] =
    "google.scp.pbs.transaction_manager.active_transactions";
static constexpr char kMetricNameReceivedTransactions[] =
    "google.scp.pbs.transaction_manager.received_transactions";
static constexpr char kMetricNameFinishedTransactions[] =
    "google.scp.pbs.transaction_manager.finished_transactions";

static constexpr char kMetricLabelPartitionId[] = "partition_id";

}  // namespace google::scp::core
