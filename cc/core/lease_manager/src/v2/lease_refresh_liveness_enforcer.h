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
#include <thread>
#include <vector>

#include "core/interface/lease_manager_interface.h"

namespace google::scp::core {
/**
 * @copydoc LeaseRefreshLivenessEnforcerInterface
 *
 * Automatic lease refresh liveness enforcer that employs an internal worker
 * thread.
 */
class LeaseRefreshLivenessEnforcer
    : public LeaseRefreshLivenessEnforcerInterface {
 public:
  LeaseRefreshLivenessEnforcer(
      const std::vector<std::shared_ptr<LeaseRefreshLivenessCheckInterface>>&
          lease_refresher_list,
      std::chrono::milliseconds lease_duration_in_milliseconds,
      std::function<void()> corrective_action_for_liveness = std::abort);

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  /**
   * @brief Performs a round of lease liveness enforcement on all the locks. If
   * any lock is found to be not live, then an error is returned.
   *
   * @return ExecutionResult
   */
  ExecutionResult PerformLeaseLivenessEnforcement() noexcept override;

 protected:
  void LivenessEnforcerThreadFunction();

  /// @brief lease freshness enforcer thread. Enforcer thread goes through all
  /// the refresher objects and ensures that are doing their work in a timely
  /// manner. If a refresher is stuck, that means a partition is loaded but its
  /// lease is not renewed, and this leads to violations in correctness. So this
  /// is maintained as a seperate thread.
  std::unique_ptr<std::thread> enforcer_thread_;
  /// @brief set of lease refreshers.
  const std::vector<std::shared_ptr<LeaseRefreshLivenessCheckInterface>>
      lease_refresher_list_;
  /// @brief lease duration
  const std::chrono::milliseconds lease_duration_in_milliseconds_;
  /// @brief enforcement interval.
  const std::chrono::milliseconds enforcement_interval_in_milliseconds_;
  /// @brief lease freshness threshold
  std::chrono::milliseconds lease_freshness_threshold_in_milliseconds_;
  /// @brief is lease enforcer thread running.
  std::atomic<bool> is_running_;
  /// @brief corrective action for ensuring liveness
  std::function<void()> corrective_action_for_liveness_;
  /// @brief mutex to serialize external requests to enforce liveness with
  /// internal thread.
  std::mutex lease_enforcer_mutex_;
  /// @brief activity ID of the run
  core::common::Uuid object_activity_id_;
};
}  // namespace google::scp::core
