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

/**
 * @brief
 * Partition Lease Sink Metrics
 *  Metric Name: kMetricNamePartitionLoadDurationMs
 *  Metric Name: kMetricNamePartitionUnloadDurationMs
 *  Metric Name: kMetricNamePartitionLeaseEvent
 *               Events: kMetricEventPartitionLeaseRenewedCount
 */
static constexpr char
    kMetricComponentNameAndPartitionNamePrefixForPartitionLeaseSink[] =
        "LeaseSink for Partition ";
static constexpr char kMetricMethodPartitionLoad[] = "Load";
static constexpr char kMetricMethodPartitionUnload[] = "Unload";
static constexpr char kMetricMethodPartitionLeaseEvent[] = "Lease Event";
static constexpr char kMetricNamePartitionLoadDurationMs[] =
    "PartitionLoadDurationMs";
static constexpr char kMetricNamePartitionUnloadDurationMs[] =
    "PartitionUnloadDurationMs";
static constexpr char kMetricNamePartitionLeaseEvent[] =
    "PartitionLeaseEventCount";
static constexpr char kMetricEventPartitionLeaseRenewedCount[] =
    "LeaseRenewedCount";

/**
 * @brief
 * Budget Key Counter Metrics
 *  Metric Name: kMetricNameBudgetKeyCount
 *               Events: kMetricEventLoadFromDBScheduled
 *               Events: kMetricEventLoadFromDBSuccess
 *               Events: kMetricEventLoadFromDBFailed
 *               Events: kMetricEventUnloadFromDBScheduled
 *               Events: kMetricEventUnloadFromDBSuccess
 *               Events: kMetricEventUnloadFromDBFailed
 */
static constexpr char kMetricComponentNameAndPartitionNamePrefixForBudgetKey[] =
    "BudgetKeyProvider for Partition ";
static constexpr char kMetricNameBudgetKeyCount[] = "BudgetKeyCounter";
static constexpr char kMetricMethodLoadUnload[] = "BudgetKey Load Unload";
static constexpr char kMetricEventLoadFromDBScheduled[] =
    "LoadFromDB Scheduled";
static constexpr char kMetricEventLoadFromDBSuccess[] = "LoadFromDB Success";
static constexpr char kMetricEventLoadFromDBFailed[] = "LoadFromDB Failed";
static constexpr char kMetricEventUnloadFromDBScheduled[] =
    "UnloadFromDB Scheduled";
static constexpr char kMetricEventUnloadFromDBSuccess[] =
    "UnloadFromDB Success";
static constexpr char kMetricEventUnloadFromDBFailed[] = "UnloadFromDB Failed";

// Instance Health Metric
static constexpr char kMetricComponentNameInstanceHealth[] = "Health";

static constexpr char kMetricNameInstanceHealthMemory[] = "MemoryUsage";

static constexpr char kMetricNameInstanceHealthFileSystemLogStorageUsage[] =
    "FileSystemUsage";

};  // namespace google::scp::pbs
