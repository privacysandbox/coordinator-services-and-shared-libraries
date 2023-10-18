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

#include "partition_lease_event_sink.h"

#include <utility>
#include <vector>

#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/partition_manager_interface.h"

#include "error_codes.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LeasableLockId;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseTransitionType;
using google::scp::core::PartitionId;
using google::scp::core::PartitionMetadata;
using google::scp::core::PartitionType;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TaskCancellationLambda;
using google::scp::core::common::TaskLambda;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using std::atomic;
using std::bind;
using std::function;
using std::make_shared;
using std::move;
using std::optional;
using std::shared_ptr;
using std::unique_lock;
using std::unordered_map;
using std::weak_ptr;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;

static constexpr seconds kTaskCancelWaitTimeInMilliseconds = seconds(1);

static constexpr char kPartitionLeaseEventSink[] = "PartitionLeaseEventSink";

static constexpr char kLocalPartitionAddressUri[] =
    "" /* local partition does not need an address */;

namespace google::scp::pbs {

using PartitionMetricsMapType =
    unordered_map<core::common::Uuid, PartitionMetricsWrapper,
                  core::common::UuidHash>;

/**
 * @brief Get the Partition Metrics Wrapper object. If object is not present it
 * is created.
 *
 * @param partition_metrics
 * @param partition_id
 * @return auto
 */
static PartitionMetricsMapType::iterator CreateOrGetPartitionMetricsWrapper(
    PartitionMetricsMapType& partition_metrics,
    const shared_ptr<cpio::MetricClientInterface>& metric_client,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const PartitionId& partition_id,
    size_t metric_aggregation_interval_milliseconds) {
  auto partition_it = partition_metrics.find(partition_id);
  if (partition_it == partition_metrics.end()) {
    auto [it, is_inserted] = partition_metrics.try_emplace(
        partition_id, metric_client, async_executor, partition_id,
        metric_aggregation_interval_milliseconds);
    if (!is_inserted) {
      SCP_ERROR(
          kPartitionLeaseEventSink, core::common::kZeroUuid,
          FailureExecutionResult(
              core::errors::SC_PARTITION_LEASE_EVENT_SINK_CANNOT_INIT_METRICS),
          "Cannot Create MetricsWrapper for Partition '%s'",
          ToString(partition_id).c_str());
      return partition_metrics.end();
    }
    partition_it = it;
    // Init and Run.
    if (auto result = partition_it->second.Init(); !result.Successful()) {
      SCP_ERROR(kPartitionLeaseEventSink, core::common::kZeroUuid, result,
                "Cannot Init() MetricsWrapper for Partition '%s'",
                ToString(partition_id).c_str());
      partition_metrics.erase(partition_it);
      return partition_metrics.end();
    }
    if (auto result = partition_it->second.Run(); !result.Successful()) {
      SCP_ERROR(kPartitionLeaseEventSink, core::common::kZeroUuid, result,
                "Cannot Run() MetricsWrapper for Partition '%s'",
                ToString(partition_id).c_str());
      partition_metrics.erase(partition_it);
      return partition_metrics.end();
    }
  }
  return partition_it;
}

void PartitionLeaseEventSink::OnPartitionLeaseRenewedMetric(
    const PartitionId& partition_id) {
  std::unique_lock lock(partition_metrics_mutex_);
  auto it = CreateOrGetPartitionMetricsWrapper(
      partition_metrics_, metric_client_, async_executor_, partition_id,
      metric_aggregation_interval_milliseconds_);
  if (it == partition_metrics_.end()) {
    return;
  }
  it->second.OnLeaseRenewed();
}

void PartitionLeaseEventSink::OnPartitionLoadMetric(
    const PartitionId& partition_id, size_t load_duration_in_ms) {
  std::unique_lock lock(partition_metrics_mutex_);
  auto it = CreateOrGetPartitionMetricsWrapper(
      partition_metrics_, metric_client_, async_executor_, partition_id,
      metric_aggregation_interval_milliseconds_);
  if (it == partition_metrics_.end()) {
    return;
  }
  it->second.OnPartitionLoaded(load_duration_in_ms);
}

void PartitionLeaseEventSink::OnPartitionUnloadMetric(
    const PartitionId& partition_id, size_t unload_duration_in_ms) {
  std::unique_lock lock(partition_metrics_mutex_);
  auto it = CreateOrGetPartitionMetricsWrapper(
      partition_metrics_, metric_client_, async_executor_, partition_id,
      metric_aggregation_interval_milliseconds_);
  if (it == partition_metrics_.end()) {
    return;
  }
  it->second.OnPartitionUnloaded(unload_duration_in_ms);
}

PartitionLeaseEventSink::ScheduledPartitionTaskWrapper::
    ScheduledPartitionTaskWrapper(TaskLambda task_lambda,
                                  PartitionTaskType task_type,
                                  const Uuid& activity_id,
                                  milliseconds startup_wait_delay)
    : task_(std::move(task_lambda), startup_wait_delay),
      task_type(task_type),
      sink_activity_id(activity_id),
      task_id(Uuid::GenerateUuid()) {
  SCP_INFO(kPartitionLeaseEventSink, activity_id,
           "Starting a task with ID: '%s' for task type: '%d', with a "
           "startup delay of '%llu' (ms)",
           ToString(task_id).c_str(), task_type, startup_wait_delay.count());
}

void PartitionLeaseEventSink::ScheduledPartitionTaskWrapper::
    CancelOrWaitUntilTaskIsDone() {
  SCP_INFO(kPartitionLeaseEventSink, sink_activity_id,
           "Cancelling task with ID: '%s'", ToString(task_id).c_str());
  // Try cancel, this may not succeed if the task is already cancelled or
  // cannot be cancelled.
  if (!task_.Cancel()) {
    SCP_INFO(
        kPartitionLeaseEventSink, sink_activity_id,
        "Task with ID: '%s' not in cancellable state. Waiting for task to be "
        "completed...",
        ToString(task_id).c_str());
    // Not succeeded, wait until the task is done.
    while (!IsTaskDone()) {
      sleep_for(kTaskCancelWaitTimeInMilliseconds);
      SCP_INFO(kPartitionLeaseEventSink, sink_activity_id,
               "Waiting for task with ID: '%s' to be finished...",
               ToString(task_id).c_str());
    }
  }
  SCP_INFO(kPartitionLeaseEventSink, sink_activity_id,
           "Task with ID: '%s' is finished.", ToString(task_id).c_str());
}

void PartitionLeaseEventSink::AbortProcess(ExecutionResult execution_result) {
  SCP_EMERGENCY(kPartitionLeaseEventSink, object_activity_id_, execution_result,
                "Terminating PBS!!!");
  // TODO: Add a metric to measure how many aborts happened.
  abort_handler_();
}

PartitionLeaseEventSink::PartitionLeaseEventSink(
    const shared_ptr<core::PartitionManagerInterface>& partition_manager,
    const shared_ptr<core::AsyncExecutorInterface>& async_executor,
    const weak_ptr<core::LeaseReleaseNotificationInterface>&
        lease_event_notification,
    const shared_ptr<cpio::MetricClientInterface>& metric_client,
    const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
    seconds partition_bootup_wait_time_in_seconds,
    function<void()> abort_handler)
    : partition_manager_(partition_manager),
      async_executor_(async_executor),
      lease_event_notification_(lease_event_notification),
      metric_client_(metric_client),
      config_provider_(config_provider),
      object_activity_id_(Uuid::GenerateUuid()),
      partition_bootup_wait_time_in_seconds_(
          partition_bootup_wait_time_in_seconds),
      abort_handler_(abort_handler),
      is_running_(false),
      metric_aggregation_interval_milliseconds_(
          core::kDefaultAggregatedMetricIntervalMs) {}

ExecutionResult PartitionLeaseEventSink::Init() noexcept {
  if (!config_provider_
           ->Get(core::kAggregatedMetricIntervalMs,
                 metric_aggregation_interval_milliseconds_)
           .Successful()) {
    metric_aggregation_interval_milliseconds_ =
        core::kDefaultAggregatedMetricIntervalMs;
  }
  return SuccessExecutionResult();
}

ExecutionResult PartitionLeaseEventSink::Run() noexcept {
  unique_lock lock(on_lease_transition_mutex_);
  is_running_ = true;
  return SuccessExecutionResult();
}

ExecutionResult PartitionLeaseEventSink::Stop() noexcept {
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_, "Stopping");
  // Do not let lease transitions happen.
  unique_lock lock(on_lease_transition_mutex_);
  is_running_ = false;
  // Cancel any pending tasks
  for (auto& [_, task_wrapper] : partition_tasks_) {
    task_wrapper.CancelOrWaitUntilTaskIsDone();
  }

  // Stop the metrics.
  for (auto& [_, metric_wrapper] : partition_metrics_) {
    RETURN_IF_FAILURE(metric_wrapper.Stop());
  }

  return SuccessExecutionResult();
}

void PartitionLeaseEventSink::LoadLocalPartitionHelper(
    const PartitionId& partition_id) {
  // TODO: Add a metric to measure the load time and track the ID of the
  // partition as well.
  PartitionMetadata partition_metadata(partition_id, PartitionType::Local,
                                       kLocalPartitionAddressUri);
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Loading LOCAL Partition '%s'...", ToString(partition_id).c_str());

  // Load the Local Partition
  auto load_start_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();
  auto execution_result = partition_manager_->LoadPartition(partition_metadata);
  if (!execution_result.Successful()) {
    // If load is unsuccessful, we need to act on this (to reduce
    // downtime) by restarting the process.
    SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
              "Loading LOCAL Partition failed.");
    return AbortProcess(execution_result);
  }
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Loaded the LOCAL Partition '%s'", ToString(partition_id).c_str());

  OnPartitionLoadMetric(partition_id,
                        duration_cast<milliseconds>(
                            TimeProvider::GetSteadyTimestampInNanoseconds() -
                            load_start_timestamp)
                            .count());
}

void PartitionLeaseEventSink::UnloadLocalPartitionHelper(
    const PartitionId& partition_id, bool should_notify_lease_manager) {
  // TODO: Add a metric to measure the unload time and track the ID of the
  // partition as well.
  // Unload the partition
  PartitionMetadata partition_metadata(partition_id, PartitionType::Local,
                                       kLocalPartitionAddressUri);
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Unloading LOCAL Partition '%s'...", ToString(partition_id).c_str());
  auto unload_start_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();
  auto execution_result =
      partition_manager_->UnloadPartition(partition_metadata);
  if (!execution_result.Successful() &&
      (execution_result !=
       FailureExecutionResult(
           core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST))) {
    // If Unload is unsuccessful, we need to act on this (to reduce
    // downtime) by restarting the process.
    SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
              "Unloading LOCAL Partition failed.");
    return AbortProcess(execution_result);
  }
  if (should_notify_lease_manager) {
    auto lease_event_notification = lease_event_notification_.lock();
    // Notifying the sink is not required if the notification destination has
    // gone out of scope.
    if (lease_event_notification) {
      lease_event_notification->SafeToReleaseLease(partition_id);
    }
  }
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Unloaded the LOCAL Partition '%s'", ToString(partition_id).c_str());
  OnPartitionUnloadMetric(partition_id,
                          duration_cast<milliseconds>(
                              TimeProvider::GetSteadyTimestampInNanoseconds() -
                              unload_start_timestamp)
                              .count());
}

void PartitionLeaseEventSink::OnLeaseNotAcquired(
    const LeasableLockId& lock_id, optional<LeaseInfo> lease_info) {
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Starting OnLeaseNotAcquired for Partition '%s'",
           ToString(lock_id).c_str());
  // ASSUMPTION:
  // Local Partition was already unloaded in LeaseLost/LeaseReleased events
  // synchronously, so no need to unload it again here.
  //
  // Load Remote partition synchronously. This is a quick operation since
  // there is no data and no need to do it asynchronously.
  //
  if (!lease_info.has_value()) {
    // If lease owner is not present, do nothing.
    SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
             "LeaseOwnerInfo not present. No operation.");
    return;
  }

  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "LeaseOwner Info ID: '%s' Endpoint: '%s'",
           lease_info->lease_acquirer_id.c_str(),
           lease_info->service_endpoint_address.c_str());

  // If remote partition is already present, then refresh the address.
  PartitionMetadata partition_metadata(lock_id, PartitionType::Remote,
                                       lease_info->service_endpoint_address);
  auto execution_result =
      partition_manager_->RefreshPartitionAddress(partition_metadata);
  if (!execution_result.Successful() &&
      (execution_result !=
       FailureExecutionResult(
           core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST))) {
    SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
              "Unable to lookup REMOTE Partition '%s' in the map.",
              ToString(lock_id).c_str());
    return AbortProcess(execution_result);
  } else if (execution_result.Successful()) {
    SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
             "REMOTE Partition '%s' already exists in the map. Refreshed the "
             "endpoint address to '%s'",
             ToString(lock_id).c_str(),
             lease_info->service_endpoint_address.c_str());
    return;
  }

  // Load the remote partition because it is not already present
  execution_result = partition_manager_->LoadPartition(partition_metadata);
  if (!execution_result.Successful()) {
    // If load is unsuccessful, we need to act on this (to reduce
    // downtime) -- restarting the process.
    SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
              "Failed to Load REMOTE Partition '%s'",
              ToString(lock_id).c_str());
    return AbortProcess(execution_result);
  }
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Loaded the REMOTE Partition '%s' with hosted address '%s'",
           ToString(lock_id).c_str(),
           lease_info->service_endpoint_address.c_str());
}

void PartitionLeaseEventSink::OnLeaseAcquired(const LeasableLockId& lock_id) {
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Starting OnLeaseAcquired for Partition '%s'",
           ToString(lock_id).c_str());
  // Check if there is any pending task
  auto task_wrapper_it = partition_tasks_.find(lock_id);
  if (task_wrapper_it != partition_tasks_.end()) {
    auto& task_wrapper = task_wrapper_it->second;
    // Any pending task must have been completed by the time
    // LeaseAcquired is issued. If we see a task, it means a previous unloading
    // is still happening or something else?
    if (!task_wrapper.IsTaskDone()) {
      auto execution_result = FailureExecutionResult(
          core::errors::
              SC_PARTITION_LEASE_EVENT_SINK_TASK_RUNNING_WHILE_ACQUIRE);
      SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
                "A Task of Type: '%d' is running for Partition '%s'",
                task_wrapper.GetTaskType(), ToString(lock_id).c_str());
      return AbortProcess(execution_result);
    }
    // Cleanup the completed one. (garbage collecting reactively)
    partition_tasks_.erase(task_wrapper_it);
  }

  // Unload remote partition. (if present)
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Unloading REMOTE partition (if any) with ID: %s",
           ToString(lock_id).c_str());
  PartitionMetadata partition_metadata(lock_id, PartitionType::Remote,
                                       "" /* not required for unload */);
  auto execution_result =
      partition_manager_->UnloadPartition(partition_metadata);
  if (!execution_result.Successful() &&
      execution_result !=
          FailureExecutionResult(
              core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)) {
    SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
              "Cannot unload a REMOTE Partition '%s'",
              ToString(lock_id).c_str());
    return AbortProcess(execution_result);
  }

  auto [inserted_it, is_inserted] = partition_tasks_.try_emplace(
      lock_id,
      bind(&PartitionLeaseEventSink::LoadLocalPartitionHelper, this, lock_id),
      PartitionTaskType::Load, object_activity_id_,
      partition_bootup_wait_time_in_seconds_);
  if (!is_inserted) {
    execution_result = FailureExecutionResult(
        core::errors::SC_PARTITION_LEASE_EVENT_SINK_CANNOT_EMPLACE_TO_MAP);
    SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
              "Cannot unload a REMOTE Partition '%s'",
              ToString(lock_id).c_str());
    return AbortProcess(execution_result);
  }
}

void PartitionLeaseEventSink::OnLeaseLost(const LeasableLockId& lock_id) {
  // Unload Local partition synchronously. This might take a while, but do it
  // in this handler which is currently run by the lease manager's lease
  // refresher thread thereby blocking the lease refresh. This allows the lease
  // manager to take the action to kill instance if taking longer to unload, and
  // which is the behavior we want if we cannot unload faster to ensure
  // correctness and safety of partition operations across two instances.

  // Check if partition is currently loading. This could happen because
  // partition load is ASYNC operation and it could take a while.
  // If partition is loading, then go ahead with a restart of the process to
  // handle the case.
  //
  // TODO: This could be done better, i.e. not restart the process but somehow
  // stop the ongoing load.
  //
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Starting OnLeaseLost for Partition '%s'",
           ToString(lock_id).c_str());

  // Check if there is a pending task (loading task)
  auto task_wrapper_it = partition_tasks_.find(lock_id);
  if (task_wrapper_it != partition_tasks_.end()) {
    auto& task_wrapper = task_wrapper_it->second;
    if (!task_wrapper.IsTaskDone() && !task_wrapper.IsCancellable()) {
      auto execution_result = FailureExecutionResult(
          core::errors::SC_PARTITION_LEASE_EVENT_SINK_TASK_RUNNING_WHILE_LOST);
      SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_, execution_result,
                "LOCAL Partition, Task of type '%d' ongoing for Partition '%s'",
                task_wrapper.GetTaskType(), ToString(lock_id).c_str());
      return AbortProcess(execution_result);
    } else {
      SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
               "LOCAL Partition, Cancelling pending Task of type '%d' of the "
               "Partition '%s'",
               task_wrapper.GetTaskType(), ToString(lock_id).c_str());
      // If the task is not running (either completed, or scheduled but not yet
      // started), then it can be cancelled.
      // There is a race: if we go ahead and try to cancel by invoking the
      // lambda, the task could start executing at the same time just before the
      // invocation (in the case of a scheduled task). For this, we go ahead
      // and block until the task completes and let lease manager enforce this
      // function if it takes too long waiting for the task to complete.
      task_wrapper.CancelOrWaitUntilTaskIsDone();
      // Cleanup the completed one.
      partition_tasks_.erase(task_wrapper_it);
    }
  }

  // Unload immediately blocking the Lease Refresher's thread. This ensures that
  // the Unload happens in a timely manner by the virtue of Lease Refresh
  // Enforcer ensuring Refresher is not blocked.
  UnloadLocalPartitionHelper(lock_id);
}

void PartitionLeaseEventSink::OnLeaseRenewed(
    const LeasableLockId& lock_id, bool should_start_releasing_lease) {
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Starting OnLeaseRenewed for Partition '%s', "
           "ShouldStartReleasingLease: '%d'",
           ToString(lock_id).c_str(), should_start_releasing_lease);

  OnPartitionLeaseRenewedMetric(lock_id);

  if (!should_start_releasing_lease) {
    // Simply report the partition's load status for debugging purposes.
    // Nothing else to do.
    auto partition_or = partition_manager_->GetPartition(lock_id);
    if (!partition_or.Successful()) {
      SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
               "Partition with ID: '%s' has not yet started loading.",
               ToString(lock_id).c_str());
    } else {
      SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
               "Partition with ID: '%s' is loading/loaded. LoadStatus of the "
               "Partition: '%d'",
               ToString(lock_id).c_str(), (*partition_or)->GetPartitionState());
    }
    return;
  }

  // Release the lease on the partition.
  // Check if there is a pending task (any non-unloading task)
  // Unloading could be in progress due to a previous OnLeaseRenewed.
  auto task_wrapper_it = partition_tasks_.find(lock_id);
  if (task_wrapper_it != partition_tasks_.end()) {
    auto& task_wrapper = task_wrapper_it->second;
    if (task_wrapper.GetTaskType() == PartitionTaskType::Unload) {
      SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
               "LOCAL Partition: An Unload task ongoing for Partition '%s'. "
               "Release ongoing. Returning.",
               ToString(lock_id).c_str());
      return;
    }
    if (task_wrapper.GetTaskType() == PartitionTaskType::Load) {
      if (!task_wrapper.IsTaskDone() && !task_wrapper.IsCancellable()) {
        SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
                 "LOCAL Partition: A Load task ongoing for Partition '%s' that "
                 "cannot be cancelled. Will Unload later after the Load "
                 "is finished. ",
                 ToString(lock_id).c_str());
        return;
      } else {
        SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
                 "LOCAL Partition: Stopping ongoing LOAD task for "
                 "Partition '%s'",
                 ToString(lock_id).c_str());
        // There is a race: if we go ahead and try to cancel by invoking the
        // lambda, the task could start executing at the same time just before
        // the invocation (in the case of a scheduled task). For this, we go
        // ahead and block until the task completes and let lease manager
        // enforce this function if it takes too long waiting for the task to
        // complete.
        task_wrapper.CancelOrWaitUntilTaskIsDone();
        // Cleanup the completed one.
        partition_tasks_.erase(task_wrapper_it);
      }
    }
  }

  // Unload partition asynchronously starting now.
  // notify lease manager after the unload is complete, because
  // should_start_releasing_lease is specified as true, i.e. Lease Manager needs
  // to get a signal back to complete the lease release of this lock.
  auto [inserted_it, is_inserted] = partition_tasks_.try_emplace(
      lock_id,
      bind(&PartitionLeaseEventSink::UnloadLocalPartitionHelper, this, lock_id,
           true /* should_notify_lease_manager */),
      PartitionTaskType::Unload, object_activity_id_);
  if (!is_inserted) {
    auto execution_result = FailureExecutionResult(
        core::errors::SC_PARTITION_LEASE_EVENT_SINK_CANNOT_EMPLACE_TO_MAP);
    return AbortProcess(execution_result);
  }
}

void PartitionLeaseEventSink::OnLeaseReleased(const LeasableLockId& lock_id) {
  SCP_INFO(kPartitionLeaseEventSink, object_activity_id_,
           "Starting OnLeaseReleased for Partition '%s'",
           ToString(lock_id).c_str());
  // No-Op.
}

void PartitionLeaseEventSink::OnLeaseTransition(
    const LeasableLockId& lock_id, LeaseTransitionType lease_transition_type,
    optional<LeaseInfo> lease_owner_info) noexcept {
  // All of the events are already serialized from the producer, but making sure
  // they are serialized on our side with a mutex.
  unique_lock lock(on_lease_transition_mutex_);
  if (!is_running_) {
    SCP_ERROR(kPartitionLeaseEventSink, object_activity_id_,
              FailureExecutionResult(
                  core::errors::SC_PARTITION_LEASE_EVENT_SINK_NOT_RUNNING),
              "PartitionLeaseEventSink not running.",
              ToString(lock_id).c_str());
    return;
  }

  // TODO: Add metric on each of these event types.
  switch (lease_transition_type) {
    case LeaseTransitionType::kAcquired:
      OnLeaseAcquired(lock_id);
      break;
    case LeaseTransitionType::kLost:
      OnLeaseLost(lock_id);
      break;
    case LeaseTransitionType::kNotAcquired:
      OnLeaseNotAcquired(lock_id, lease_owner_info);
      break;
    case LeaseTransitionType::kReleased:
      OnLeaseReleased(lock_id);
      break;
    case LeaseTransitionType::kRenewed:
      OnLeaseRenewed(lock_id,
                     false /* should start releasing existing lease */);
      break;
    case LeaseTransitionType::kRenewedWithIntentionToRelease:
      OnLeaseRenewed(lock_id, true /* should start releasing existing lease */);
      break;
    default:
      // Ignore any other type of event.
      break;
  }
}
}  // namespace google::scp::pbs
