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

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/common/cancellable_thread_task/src/cancellable_thread_task.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/lease_manager_interface.h"
#include "core/interface/partition_manager_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

#include "partition_metrics_wrapper.h"

namespace google::scp::pbs {
/**
 * @brief This class implements the LeaseEventSinkInterface to listen to events
 * generated by LeaseManager, and populates Partition Manager accordingly. This
 * also communicates back with Lease Manager via
 * LeaseReleaseNotificationInterface to notify that a lease can be safely
 * released (in graceful lease release scenarios) as a reaction to
 * LeaseTransitionType::kRenewedWithIntentionToRelease event.
 *
 * Before starting to boot up a partition, we must wait for a lease duration
 * worth of time (partition_lease_acquired_bootup_wait_time) to ensure the
 * previous lease owner has given up completely on the partition.
 */
class PartitionLeaseEventSink : public core::LeaseEventSinkInterface,
                                public core::ServiceInterface {
 public:
  // NOTE: AsyncExecutor is used for metrics only.
  PartitionLeaseEventSink(
      const std::shared_ptr<core::PartitionManagerInterface>&,
      const std::shared_ptr<core::AsyncExecutorInterface>&,
      const std::weak_ptr<core::LeaseReleaseNotificationInterface>&,
      const std::shared_ptr<cpio::MetricClientInterface>&,
      const std::shared_ptr<core::ConfigProviderInterface>&,
      std::chrono::seconds partition_lease_acquired_bootup_wait_time,
      std::function<void()> abort_handler = []() {
        // Abort the process
        std::abort();
      });

  /**
   * @brief Implementation of LeaseEventSinkInterface
   * This receives events from LeaseManager and reacts to them by
   * Loading/Unloading relevant partitions and partition types.
   *
   * IMPORTANT: This should be quick and MUST not block so that liveness is
   * ensured in lease manager.
   */
  void OnLeaseTransition(const core::LeasableLockId&, core::LeaseTransitionType,
                         std::optional<core::LeaseInfo>) noexcept override;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

 protected:
  /**
   * @brief Helper to react to a lease renewed event from lease manager.
   *
   * @param lease_lock_id
   * @param should_start_releasing_lease
   */
  void OnLeaseRenewed(const core::LeasableLockId& lease_lock_id,
                      bool should_start_releasing_lease);

  /**
   * @brief Helper to react to lease released event from lease manager.
   *
   * @param lock_id
   */
  void OnLeaseReleased(const core::LeasableLockId& lock_id);

  /**
   * @brief Helper to react to a lease not acquired event from lease
   * manager.
   *
   * @param lease_lock_id
   * @param lease_info
   */
  void OnLeaseNotAcquired(const core::LeasableLockId& lease_lock_id,
                          std::optional<core::LeaseInfo> lease_info);

  /**
   * @brief Helper to react to a lease lost event from lease manager.
   *
   * @param lease_lock_id
   */
  void OnLeaseLost(const core::LeasableLockId& lease_lock_id);

  /**
   * @brief Helper to react to a lease acquired event from lease manager.
   *
   * @param lease_lock_id
   */
  void OnLeaseAcquired(const core::LeasableLockId& lease_lock_id);

  /**
   * @brief Abort the current process.
   *
   */
  void AbortProcess(core::ExecutionResult);

  /**
   * @brief Helper to perform loading the partition.
   *
   * @param partition_id
   */
  void LoadLocalPartitionHelper(const core::PartitionId& partition_id);

  /**
   * @brief Helper to perform unloading the partition.
   *
   * @param partition_id
   * @param should_notify_lease_manager after the unload is done, should
   * we notify the lease manager that the lease is safe to be released. This is
   * applicable if unload is happening as a reaction to the event
   * kRenewedWithIntentionToRelease
   */
  void UnloadLocalPartitionHelper(const core::PartitionId& partition_id,
                                  bool should_notify_lease_manager = false);

  /**
   * @brief Represents the two types of tasks this component schedules on
   * AsyncExecutor
   */
  enum class PartitionTaskType { Load, Unload };

  void OnPartitionLeaseRenewedMetric(const core::PartitionId& partition_id);

  void OnPartitionLoadMetric(const core::PartitionId& partition_id,
                             size_t load_duration_in_ms);

  void OnPartitionUnloadMetric(const core::PartitionId& partition_id,
                               size_t unload_duration_in_ms);

  /**
   * @brief Bookkeeping structure to keep track of Load/Unload tasks that are
   * created by this component
   */
  struct ScheduledPartitionTaskWrapper {
    ScheduledPartitionTaskWrapper(core::common::TaskLambda task_lambda,
                                  PartitionTaskType task_type,
                                  const core::common::Uuid& activity_id,
                                  std::chrono::milliseconds startup_wait_delay =
                                      std::chrono::milliseconds(0));

    bool IsTaskDone() const {
      return task_.IsCancelled() || task_.IsCompleted();
    }

    bool IsCancellable() const { return task_.IsCancellable(); }

    PartitionTaskType GetTaskType() const { return task_type; }

    void CancelOrWaitUntilTaskIsDone();

    /// @brief cancellable task
    core::common::CancellableThreadTask task_;
    /// @brief type of task
    PartitionTaskType task_type;
    /// @brief activity ID of the parent object i.e. the sink.
    core::common::Uuid sink_activity_id;
    /// @brief ID of the task object
    core::common::Uuid task_id;
  };

  /// @brief Partition manager to host partitions in response to events.
  std::shared_ptr<core::PartitionManagerInterface> partition_manager_;

  /// @brief Used by the partition_metrics_.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// @brief Event notification interface to communicate that a lease is safe to
  /// be released upon encountering 'kRenewedWithIntentionToRelease' lease
  /// transition event
  /// This is a weak_ptr to not hold owning reference as the notification is
  /// optional and is only sent if the destination is available.
  std::weak_ptr<core::LeaseReleaseNotificationInterface>
      lease_event_notification_;

  /// @brief metric client
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  /// @brief config provider
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// @brief activity ID of the run
  core::common::Uuid object_activity_id_;

  /// @brief set of partition IDs and their current task cancellation hooks
  std::unordered_map<core::common::Uuid, ScheduledPartitionTaskWrapper,
                     core::common::UuidHash>
      partition_tasks_;

  /// @brief wait time before partition can be loaded and served
  std::chrono::seconds partition_bootup_wait_time_in_seconds_;

  /// @brief handler to perform abort of current process.
  std::function<void()> abort_handler_;

  /// @brief lease transition mutex.
  std::mutex on_lease_transition_mutex_;

  /// @brief Is component running
  std::atomic<bool> is_running_;

  /// @brief set of partition IDs and their metrics
  std::unordered_map<core::common::Uuid, PartitionMetricsWrapper,
                     core::common::UuidHash>
      partition_metrics_;
  /// @brief protects partition_metrics_;
  std::mutex partition_metrics_mutex_;

  /// @brief metrics aggregation interval
  size_t metric_aggregation_interval_milliseconds_;
};

}  // namespace google::scp::pbs
