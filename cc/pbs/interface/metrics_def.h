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
 *  Metric Name: kMetricNameBudgetKeyScheduledLoads
 *  Metric Name: kMetricNameBudgetKeyLoads
 *               Labels: kMetricLabelBudgetKeyLoadSuccess
 *  Metric Name: kMetricNameBudgetKeyScheduledUnloads
 *  Metric Name: kMetricNameBudgetKeyUnloads
 *               Labels: kMetricLabelBudgetKeyUnloadSuccess
 *  Metric Name: kMetricNameBudgetKeyCount
 *               Events: kMetricEventLoadFromDBScheduled
 *               Events: kMetricEventLoadFromDBSuccess
 *               Events: kMetricEventLoadFromDBFailed
 *               Events: kMetricEventUnloadFromDBScheduled
 *               Events: kMetricEventUnloadFromDBSuccess
 *               Events: kMetricEventUnloadFromDBFailed
 */
static constexpr char kMetricNameBudgetKeyScheduledLoads[] =
    "google.scp.pbs.budget_key.scheduled_loads";
static constexpr char kMetricNameBudgetKeyLoads[] =
    "google.scp.pbs.budget_key.loads";
static constexpr char kMetricNameBudgetKeyScheduledUnloads[] =
    "google.scp.pbs.budget_key.scheduled_unloads";
static constexpr char kMetricNameBudgetKeyUnloads[] =
    "google.scp.pbs.budget_key.unloads";

static constexpr char kMetricLabelBudgetKeyLoadSuccess[] = "load_success";
static constexpr char kMetricLabelBudgetKeyUnloadSuccess[] = "unload_success";

};  // namespace google::scp::pbs
