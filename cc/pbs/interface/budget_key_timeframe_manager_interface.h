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

#include <atomic>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "core/interface/checkpoint_service_interface.h"
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {
/**
 * @brief Responsible to keep the time_bucket info and number of tokens
 * associated with the time bucket.
 */
struct BudgetKeyTimeframe {
  explicit BudgetKeyTimeframe(TimeBucket time_bucket_index)
      : time_bucket_index(time_bucket_index),
        token_count(0),
        active_transaction_id(core::common::kZeroUuid),
        active_token_count(0) {}

  /// This is hour index of time bucket within time group
  const TimeBucket time_bucket_index;

  /// The actual remaining count of tokens remaining in the frame.
  std::atomic<TokenCount> token_count;

  /// The active transaction id loading or changing the current metadata.
  std::atomic<core::common::Uuid> active_transaction_id;

  /**
   * @brief In the case of a write transaction the active token count will be
   * the value that is proposed by the transaction.
   */
  std::atomic<TokenCount> active_token_count;
};

/**
 * @brief Responsible to keep the time_groups info.
 */
struct BudgetKeyTimeframeGroup : public core::LoadableObject {
  explicit BudgetKeyTimeframeGroup(TimeBucket time_group)
      : time_group(time_group) {}

  /// This is date/time in Timestamp floor to the nearest month.
  const TimeGroup time_group;

  /// A map between the budget keys and time groups.
  core::common::ConcurrentMap<TimeBucket, std::shared_ptr<BudgetKeyTimeframe>>
      budget_key_timeframes;
};

/// The request object to load budget key frame(s).
struct LoadBudgetKeyTimeframeRequest {
  /// Reporting timestamps for which the respective time buckets need to be
  /// loaded.
  /// The reporting time(s) should point to unique time buckets
  std::vector<core::Timestamp> reporting_times;
};

/// The response object after loading budget key frame(s).
struct LoadBudgetKeyTimeframeResponse {
  /// The pointer to the budget key frame(s) corresponding to reporting
  /// timestamps specified in the LoadBudgetKeyTimeframeRequest
  /// The number of `budget_key_frames` will be equal to the number of
  /// `reporting_times` of the LoadBudgetKeyTimeframeRequest.
  std::vector<std::shared_ptr<BudgetKeyTimeframe>> budget_key_frames;
};

/// Information of budget key timeframe to be modified
struct BudgetKeyTimeframeUpdateInfo {
  /// Timebucket of the reporting timestamp to be updated.
  core::Timestamp reporting_time = 0;
  /// The active transaction id.
  core::common::Uuid active_transaction_id;
  /// The active token count.
  TokenCount active_token_count;
  /// The token count.
  TokenCount token_count;
};

/// The request object to update budget key timeframe(s).
struct UpdateBudgetKeyTimeframeRequest {
  /// Time frame(s) to be updated.
  /// Time frame(s) must point to unique time buckets.
  std::vector<BudgetKeyTimeframeUpdateInfo> timeframes_to_update;
};

/// The response object after a budget key timeframe has been updated.
struct UpdateBudgetKeyTimeframeResponse {};

/**
 * @brief Is responsible to load key time frame related into from the undelying
 * storage for any specific keys.
 */
class BudgetKeyTimeframeManagerInterface : public core::ServiceInterface {
 public:
  virtual ~BudgetKeyTimeframeManagerInterface() = default;

  /**
   * @brief Returns success execution result if the budget key timeframe manager
   * can be cleanly unloaded. This must be a sync operation.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult CanUnload() noexcept = 0;

  /**
   * @brief To read/write any budget frames, the load function must be called
   * first. This method is thread-safe and can be used to ensure that any
   * Timeframes will only load once from the underlying storage systems.
   *
   * @param load_budget_key_timeframe_context the context for loading a budget
   * key.
   * @return core::ExecutionResult the execution result of the operation.
   */
  virtual core::ExecutionResult Load(
      core::AsyncContext<LoadBudgetKeyTimeframeRequest,
                         LoadBudgetKeyTimeframeResponse>&
          load_budget_key_timeframe_context) noexcept = 0;

  /**
   * @brief Updates a budget key value in the cache and ensures the change is
   * tracked.
   *
   * @param update_budget_key_timeframe_context The context of the update budget
   * key timeframe operation.
   * @return core::ExecutionResult the execution result of the operation.
   */
  virtual core::ExecutionResult Update(
      core::AsyncContext<UpdateBudgetKeyTimeframeRequest,
                         UpdateBudgetKeyTimeframeResponse>&
          update_budget_key_timeframe_context) noexcept = 0;

  /**
   * @brief Returns the id of the current budget key timeframe manager.
   *
   * @return const core::common::Uuid The budget key timeframe manager id.
   */
  virtual const core::common::Uuid GetId() noexcept = 0;

  /**
   * @brief Creates a checkpoint of the current transaction manager state.
   *
   * @param checkpoint_logs The vector of checkpoint logs.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>&
          checkpoint_logs) noexcept = 0;
};
}  // namespace google::scp::pbs
